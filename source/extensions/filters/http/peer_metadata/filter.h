// Copyright Istio Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "source/extensions/filters/http/common/factory_base.h"
#include "source/extensions/filters/http/common/pass_through_filter.h"
#include "source/extensions/filters/http/peer_metadata/config.pb.h"
#include "source/extensions/filters/http/peer_metadata/config.pb.validate.h"
#include "source/extensions/common/workload_discovery/api.h"
#include "source/common/singleton/const_singleton.h"
#include "extensions/common/context.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace PeerMetadata {

constexpr absl::string_view WasmDownstreamPeer = "wasm.downstream_peer";
constexpr absl::string_view WasmDownstreamPeerID = "wasm.downstream_peer_id";
constexpr absl::string_view WasmUpstreamPeer = "wasm.upstream_peer";
constexpr absl::string_view WasmUpstreamPeerID = "wasm.upstream_peer_id";

struct HeaderValues {
  const Http::LowerCaseString Baggage{"baggage"};
  const Http::LowerCaseString ExchangeMetadataHeader{"x-envoy-peer-metadata"};
  const Http::LowerCaseString ExchangeMetadataHeaderId{"x-envoy-peer-metadata-id"};
};

using Headers = ConstSingleton<HeaderValues>;

// Peer info in the flatbuffers format.
using PeerInfo = std::string;

// Base class for the discovery methods. First derivation wins but all methods perform removal.
class DiscoveryMethod {
public:
  virtual ~DiscoveryMethod() = default;
  virtual absl::optional<PeerInfo> derivePeerInfo(const StreamInfo::StreamInfo&,
                                                  Http::HeaderMap&) const PURE;
  virtual void remove(Http::HeaderMap&) const {}
};

using DiscoveryMethodPtr = std::unique_ptr<DiscoveryMethod>;

// Base class for the propagation methods.
class PropagationMethod {
public:
  virtual ~PropagationMethod() = default;
  virtual void inject(Http::HeaderMap&) const PURE;
};

using PropagationMethodPtr = std::unique_ptr<PropagationMethod>;

class MXPropagationMethod : public PropagationMethod {
public:
  MXPropagationMethod(Server::Configuration::ServerFactoryContext& factory_context);
  void inject(Http::HeaderMap&) const override;

private:
  const std::string id_;
  std::string value_;
};

class FilterConfig {
public:
  FilterConfig(const io::istio::http::peer_metadata::Config&,
               Server::Configuration::FactoryContext&);
  void discoverDownstream(StreamInfo::StreamInfo&, Http::RequestHeaderMap&) const;
  void discoverUpstream(StreamInfo::StreamInfo&, Http::ResponseHeaderMap&) const;
  void injectDownstream(Http::ResponseHeaderMap&) const;
  void injectUpstream(Http::RequestHeaderMap&) const;

private:
  std::vector<DiscoveryMethodPtr> buildDiscoveryMethods(
      const Protobuf::RepeatedPtrField<io::istio::http::peer_metadata::Config::DiscoveryMethod>&,
      bool downstream, Server::Configuration::FactoryContext&) const;
  std::vector<PropagationMethodPtr> buildPropagationMethods(
      const Protobuf::RepeatedPtrField<io::istio::http::peer_metadata::Config::PropagationMethod>&,
      Server::Configuration::FactoryContext&) const;
  StreamInfo::StreamSharingMayImpactPooling sharedWithUpstream() const {
    return shared_with_upstream_
               ? StreamInfo::StreamSharingMayImpactPooling::SharedWithUpstreamConnectionOnce
               : StreamInfo::StreamSharingMayImpactPooling::None;
  }
  void discover(StreamInfo::StreamInfo&, bool downstream, Http::HeaderMap&) const;
  void setFilterState(StreamInfo::StreamInfo&, bool downstream, const std::string& value) const;
  const bool shared_with_upstream_;
  const std::vector<DiscoveryMethodPtr> downstream_discovery_;
  const std::vector<DiscoveryMethodPtr> upstream_discovery_;
  const std::vector<PropagationMethodPtr> downstream_propagation_;
  const std::vector<PropagationMethodPtr> upstream_propagation_;
};

using FilterConfigSharedPtr = std::shared_ptr<FilterConfig>;

class Filter : public Http::PassThroughFilter {
public:
  Filter(const FilterConfigSharedPtr& config) : config_(config) {}
  Http::FilterHeadersStatus decodeHeaders(Http::RequestHeaderMap&, bool) override;
  Http::FilterHeadersStatus encodeHeaders(Http::ResponseHeaderMap&, bool) override;

private:
  FilterConfigSharedPtr config_;
};

class FilterConfigFactory : public Common::FactoryBase<io::istio::http::peer_metadata::Config> {
public:
  FilterConfigFactory() : FactoryBase("envoy.filters.http.peer_metadata") {}

private:
  Http::FilterFactoryCb
  createFilterFactoryFromProtoTyped(const io::istio::http::peer_metadata::Config&,
                                    const std::string&,
                                    Server::Configuration::FactoryContext&) override;
};

} // namespace PeerMetadata
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
