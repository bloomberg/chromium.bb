// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/network_service_test_helper.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "content/public/common/content_features.h"
#include "content/public/test/test_host_resolver.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {

class NetworkServiceTestHelper::NetworkServiceTestImpl
    : public content::mojom::NetworkServiceTest {
 public:
  NetworkServiceTestImpl() = default;
  ~NetworkServiceTestImpl() override = default;

  // content::mojom::NetworkServiceTest:
  void AddRules(std::vector<content::mojom::RulePtr> rules,
                AddRulesCallback callback) override {
    for (const auto& rule : rules) {
      test_host_resolver_.host_resolver()->AddRule(rule->host_pattern,
                                                   rule->replacement);
    }
    std::move(callback).Run();
  }

  void BindRequest(content::mojom::NetworkServiceTestRequest request) {
    bindings_.AddBinding(this, std::move(request));
  }

 private:
  mojo::BindingSet<content::mojom::NetworkServiceTest> bindings_;
  content::TestHostResolver test_host_resolver_;

  DISALLOW_COPY_AND_ASSIGN(NetworkServiceTestImpl);
};

NetworkServiceTestHelper::NetworkServiceTestHelper() = default;

NetworkServiceTestHelper::~NetworkServiceTestHelper() = default;

void NetworkServiceTestHelper::RegisterNetworkBinders(
    service_manager::BinderRegistry* registry) {
  if (!base::FeatureList::IsEnabled(features::kNetworkService))
    return;

  registry->AddInterface(
      base::Bind(&NetworkServiceTestHelper::BindNetworkServiceTestRequest,
                 base::Unretained(this)));

  // Register the EmbeddedTestServer's certs, so that any SSL connections to it
  // succeed.
  net::EmbeddedTestServer::RegisterTestCerts();
}

void NetworkServiceTestHelper::BindNetworkServiceTestRequest(
    content::mojom::NetworkServiceTestRequest request) {
  if (!network_service_test_impl_)
    network_service_test_impl_ = std::make_unique<NetworkServiceTestImpl>();
  network_service_test_impl_->BindRequest(std::move(request));
}

}  // namespace content
