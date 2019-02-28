// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/dns_probe_test_util.h"

#include <stdint.h>
#include <utility>

#include "chrome/browser/net/dns_probe_runner.h"
#include "net/base/ip_address.h"

namespace chrome_browser_net {

namespace {

static base::Optional<net::AddressList> AddressListForResponse(
    FakeHostResolver::Response response) {
  base::Optional<net::AddressList> resolved_addresses;
  switch (response) {
    case FakeHostResolver::kNoResponse:
      break;
    case FakeHostResolver::kEmptyResponse:
      resolved_addresses = net::AddressList();
      break;
    case FakeHostResolver::kOneAddressResponse:
      resolved_addresses =
          net::AddressList(net::IPEndPoint(net::IPAddress(192, 168, 1, 1), 0));
      break;
  }
  return resolved_addresses;
}

}  // namespace

FakeHostResolver::FakeHostResolver(
    network::mojom::HostResolverRequest resolver_request,
    std::vector<SingleResult> result_list)
    : binding_(this, std::move(resolver_request)), result_list_(result_list) {}

FakeHostResolver::FakeHostResolver(
    network::mojom::HostResolverRequest resolver_request,
    int32_t result,
    Response response)
    : FakeHostResolver(std::move(resolver_request),
                       {SingleResult(result, response)}) {}

FakeHostResolver::~FakeHostResolver() = default;

void FakeHostResolver::ResolveHost(
    const net::HostPortPair& host,
    network::mojom::ResolveHostParametersPtr optional_parameters,
    network::mojom::ResolveHostClientPtr response_client) {
  const SingleResult& cur_result = result_list_[next_result_];
  if (next_result_ + 1 < result_list_.size())
    next_result_++;
  response_client->OnComplete(cur_result.result,
                              AddressListForResponse(cur_result.response));
}

void FakeHostResolver::MdnsListen(
    const net::HostPortPair& host,
    net::DnsQueryType query_type,
    network::mojom::MdnsListenClientPtr response_client,
    MdnsListenCallback callback) {
  NOTREACHED();
}

HangingHostResolver::HangingHostResolver(
    network::mojom::HostResolverRequest resolver_request)
    : binding_(this, std::move(resolver_request)) {}

HangingHostResolver::~HangingHostResolver() = default;

void HangingHostResolver::ResolveHost(
    const net::HostPortPair& host,
    network::mojom::ResolveHostParametersPtr optional_parameters,
    network::mojom::ResolveHostClientPtr response_client) {
  // Intentionally do not call response_client->OnComplete, but hang onto the
  // |response_client| since destroying that also causes the mojo
  // set_connection_error_handler handler to be called.
  response_client_ = std::move(response_client);
}

void HangingHostResolver::MdnsListen(
    const net::HostPortPair& host,
    net::DnsQueryType query_type,
    network::mojom::MdnsListenClientPtr response_client,
    MdnsListenCallback callback) {
  NOTREACHED();
}

FakeHostResolverNetworkContext::FakeHostResolverNetworkContext(
    std::vector<FakeHostResolver::SingleResult> system_result_list,
    std::vector<FakeHostResolver::SingleResult> public_result_list)
    : system_result_list_(std::move(system_result_list)),
      public_result_list_(std::move(public_result_list)) {}

FakeHostResolverNetworkContext::~FakeHostResolverNetworkContext() = default;

void FakeHostResolverNetworkContext::CreateHostResolver(
    const base::Optional<net::DnsConfigOverrides>& config_overrides,
    network::mojom::HostResolverRequest request) {
  ASSERT_TRUE(config_overrides);
  if (!config_overrides->nameservers) {
    if (!system_resolver_) {
      system_resolver_ = std::make_unique<FakeHostResolver>(
          std::move(request), system_result_list_);
    }
  } else {
    if (!public_resolver_) {
      public_resolver_ = std::make_unique<FakeHostResolver>(
          std::move(request), public_result_list_);
    }
  }
}

FakeDnsConfigChangeManager::FakeDnsConfigChangeManager(
    network::mojom::DnsConfigChangeManagerRequest request)
    : binding_(this, std::move(request)) {}

FakeDnsConfigChangeManager::~FakeDnsConfigChangeManager() = default;

void FakeDnsConfigChangeManager::RequestNotifications(
    network::mojom::DnsConfigChangeManagerClientPtr client) {
  ASSERT_FALSE(client_);
  client_ = std::move(client);
}

void FakeDnsConfigChangeManager::SimulateDnsConfigChange() {
  ASSERT_TRUE(client_);
  client_->OnSystemDnsConfigChanged();
}

}  // namespace chrome_browser_net
