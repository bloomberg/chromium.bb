// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_DNS_PROBE_TEST_UTIL_H_
#define CHROME_BROWSER_NET_DNS_PROBE_TEST_UTIL_H_

#include <memory>
#include <vector>

#include "mojo/public/cpp/bindings/binding.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "services/network/test/test_network_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_browser_net {

class FakeHostResolver : public network::mojom::HostResolver {
 public:
  enum Response {
    kNoResponse = 0,
    kEmptyResponse = 1,
    kOneAddressResponse = 2,
  };

  struct SingleResult {
    SingleResult(int32_t result, Response response)
        : result(result), response(response) {}
    int32_t result;
    Response response;
  };

  FakeHostResolver(network::mojom::HostResolverRequest resolver_request,
                   std::vector<SingleResult> result_list);

  FakeHostResolver(network::mojom::HostResolverRequest resolver_request,
                   int32_t result,
                   Response response);

  ~FakeHostResolver() override;

  void ResolveHost(
      const net::HostPortPair& host,
      network::mojom::ResolveHostParametersPtr optional_parameters,
      network::mojom::ResolveHostClientPtr response_client) override;

  void MdnsListen(const net::HostPortPair& host,
                  net::DnsQueryType query_type,
                  network::mojom::MdnsListenClientPtr response_client,
                  MdnsListenCallback callback) override;

 private:
  mojo::Binding<network::mojom::HostResolver> binding_;
  std::vector<SingleResult> result_list_;
  size_t next_result_ = 0;
};

class HangingHostResolver : public network::mojom::HostResolver {
 public:
  explicit HangingHostResolver(
      network::mojom::HostResolverRequest resolver_request);
  ~HangingHostResolver() override;

  void ResolveHost(
      const net::HostPortPair& host,
      network::mojom::ResolveHostParametersPtr optional_parameters,
      network::mojom::ResolveHostClientPtr response_client) override;

  void MdnsListen(const net::HostPortPair& host,
                  net::DnsQueryType query_type,
                  network::mojom::MdnsListenClientPtr response_client,
                  MdnsListenCallback callback) override;

 private:
  mojo::Binding<network::mojom::HostResolver> binding_;
  network::mojom::ResolveHostClientPtr response_client_;
};

class FakeHostResolverNetworkContext : public network::TestNetworkContext {
 public:
  FakeHostResolverNetworkContext(
      std::vector<FakeHostResolver::SingleResult> system_result_list,
      std::vector<FakeHostResolver::SingleResult> public_result_list);
  ~FakeHostResolverNetworkContext() override;

  void CreateHostResolver(
      const base::Optional<net::DnsConfigOverrides>& config_overrides,
      network::mojom::HostResolverRequest request) override;

 private:
  std::vector<FakeHostResolver::SingleResult> system_result_list_;
  std::vector<FakeHostResolver::SingleResult> public_result_list_;
  std::unique_ptr<FakeHostResolver> system_resolver_;
  std::unique_ptr<FakeHostResolver> public_resolver_;
};

class FakeDnsConfigChangeManager
    : public network::mojom::DnsConfigChangeManager {
 public:
  explicit FakeDnsConfigChangeManager(
      network::mojom::DnsConfigChangeManagerRequest request);
  ~FakeDnsConfigChangeManager() override;

  // mojom::DnsConfigChangeManager implementation:
  void RequestNotifications(
      network::mojom::DnsConfigChangeManagerClientPtr client) override;

  void SimulateDnsConfigChange();

 private:
  mojo::Binding<network::mojom::DnsConfigChangeManager> binding_;
  network::mojom::DnsConfigChangeManagerClientPtr client_;
};

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_DNS_PROBE_TEST_UTIL_H_
