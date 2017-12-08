// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/network_service.mojom.h"
#include "content/public/common/network_service_test.mojom.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/simple_url_loader_test_helper.h"
#include "content/shell/browser/shell.h"
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

    base::RunLoop run_loop;
    network_service_test.set_connection_error_handler(run_loop.QuitClosure());

    network_service_test->SimulateCrash();
    run_loop.Run();
  }

  int LoadBasicRequest(mojom::NetworkContext* network_context) {
    mojom::URLLoaderFactoryPtr url_loader_factory;
    network_context->CreateURLLoaderFactory(MakeRequest(&url_loader_factory),
                                            0);
    // |url_loader_factory| will receive error notification asynchronously if
    // |network_context| has already encountered error. However it's still false
    // at this point.
    EXPECT_FALSE(url_loader_factory.encountered_error());

    std::unique_ptr<ResourceRequest> request =
        std::make_unique<ResourceRequest>();
    // Use '/echoheader' instead of '/echo' to avoid a disk_cache bug.
    // See https://crbug.com/792255.
    request->url = embedded_test_server()->GetURL("/echoheader");

    content::SimpleURLLoaderTestHelper simple_loader_helper;
    std::unique_ptr<content::SimpleURLLoader> simple_loader =
        content::SimpleURLLoader::Create(std::move(request),
                                         TRAFFIC_ANNOTATION_FOR_TESTS);
    simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        url_loader_factory.get(), simple_loader_helper.GetCallback());
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

  // Crash the NetworkService process. Existing interfaces should receive error
  // notifications at some point.
  SimulateNetworkServiceCrash();
  // |network_context| will receive an error notification, but it's not
  // guaranteed to have arrived at this point. Flush the pointer to make sure
  // the notification has been received.
  network_context.FlushForTesting();
  EXPECT_TRUE(network_context.is_bound());
  EXPECT_TRUE(network_context.encountered_error());
  // Make sure we could get |net::ERR_FAILED| with an invalid |network_context|.
  EXPECT_EQ(net::ERR_FAILED, LoadBasicRequest(network_context.get()));

  // NetworkService should restart automatically and return valid interface.
  mojom::NetworkContextPtr network_context2 = CreateNetworkContext();
  EXPECT_EQ(net::OK, LoadBasicRequest(network_context2.get()));
  EXPECT_TRUE(network_context2.is_bound());
  EXPECT_FALSE(network_context2.encountered_error());
}

// Flaky on Mac bots. crbug.com/793296
#if defined(OS_MACOSX)
#define MAYBE_StoragePartitionImplGetNetworkContext \
  DISABLED_StoragePartitionImplGetNetworkContext
#else
#define MAYBE_StoragePartitionImplGetNetworkContext \
  StoragePartitionImplGetNetworkContext
#endif
// Make sure |StoragePartitionImpl::GetNetworkContext()| returns valid interface
// after crash.
IN_PROC_BROWSER_TEST_F(NetworkServiceRestartBrowserTest,
                       MAYBE_StoragePartitionImplGetNetworkContext) {
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(
          shell()->web_contents()->GetBrowserContext()));

  mojom::NetworkContext* old_network_context = partition->GetNetworkContext();
  EXPECT_EQ(net::OK, LoadBasicRequest(old_network_context));

  // Crash the NetworkService process. Existing interfaces should receive error
  // notifications at some point.
  SimulateNetworkServiceCrash();
  // Flush the interface to make sure the error notification was received.
  partition->FlushNetworkInterfaceForTesting();

  // |partition->GetNetworkContext()| should return a valid new pointer after
  // crash.
  EXPECT_NE(old_network_context, partition->GetNetworkContext());
  EXPECT_EQ(net::OK, LoadBasicRequest(partition->GetNetworkContext()));
}

}  // namespace content
