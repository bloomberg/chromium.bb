// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/content_features.h"
#include "content/public/common/network_service.mojom.h"
#include "content/public/common/network_service_test.mojom.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/simple_url_loader_test_helper.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/service_manager/public/cpp/connector.h"

namespace content {

namespace {

mojom::NetworkContextPtr CreateNetworkContext() {
  mojom::NetworkContextPtr network_context;
  mojom::NetworkContextParamsPtr context_params =
      mojom::NetworkContextParams::New();
  GetNetworkService()->CreateNetworkContext(mojo::MakeRequest(&network_context),
                                            std::move(context_params));
  return network_context;
}

}  // namespace

// This test source has been excluded from Android as Android doesn't have
// out-of-process Network Service.
class NetworkServiceRestartBrowserTest : public ContentBrowserTest {
 public:
  NetworkServiceRestartBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kNetworkService);
    EXPECT_TRUE(embedded_test_server()->Start());
  }

  void SimulateNetworkServiceCrash() {
    mojom::NetworkServiceTestPtr network_service_test;
    ServiceManagerConnection::GetForProcess()->GetConnector()->BindInterface(
        mojom::kNetworkServiceName, &network_service_test);
    network_service_test->SimulateCrash();
    network_service_test.FlushForTesting();
  }

  int LoadBasicRequest(mojom::NetworkContext* network_context) {
    mojom::URLLoaderFactoryPtr url_loader_factory;
    network_context->CreateURLLoaderFactory(MakeRequest(&url_loader_factory),
                                            0);

    content::SimpleURLLoaderTestHelper simple_loader_helper;
    std::unique_ptr<content::SimpleURLLoader> simple_loader =
        content::SimpleURLLoader::Create();

    ResourceRequest request;
    request.url = embedded_test_server()->GetURL("/echo");

    simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        request, url_loader_factory.get(), TRAFFIC_ANNOTATION_FOR_TESTS,
        simple_loader_helper.GetCallback());
    simple_loader_helper.WaitForCallback();

    return simple_loader->NetError();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(NetworkServiceRestartBrowserTest);
};

IN_PROC_BROWSER_TEST_F(NetworkServiceRestartBrowserTest,
                       NetworkServiceProcessRecovery) {
  mojom::NetworkContextPtr network_context = CreateNetworkContext();
  EXPECT_EQ(net::OK, LoadBasicRequest(network_context.get()));
  EXPECT_TRUE(network_context.is_bound());
  EXPECT_FALSE(network_context.encountered_error());

  // Crash the NetworkService process. Existing interfaces should observe
  // connection errors.
  SimulateNetworkServiceCrash();
  EXPECT_EQ(net::ERR_FAILED, LoadBasicRequest(network_context.get()));
  EXPECT_TRUE(network_context.is_bound());
  EXPECT_TRUE(network_context.encountered_error());

  // NetworkService should restart automatically and return valid interface.
  mojom::NetworkContextPtr network_context2 = CreateNetworkContext();
  EXPECT_EQ(net::OK, LoadBasicRequest(network_context2.get()));
  EXPECT_TRUE(network_context2.is_bound());
  EXPECT_FALSE(network_context2.encountered_error());
}

}  // namespace content
