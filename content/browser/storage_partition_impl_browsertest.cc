// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/storage_partition_impl.h"

#include <string>

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/test_url_loader_client.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "net/http/http_response_headers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_response_info.h"
#include "services/network/public/interfaces/network_service.mojom.h"
#include "services/network/public/interfaces/url_loader.mojom.h"
#include "services/network/public/interfaces/url_loader_factory.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

enum class NetworkServiceState {
  kDisabled,
  kEnabled,
};

class StoragePartititionImplBrowsertest
    : public ContentBrowserTest,
      public testing::WithParamInterface<NetworkServiceState> {
 public:
  StoragePartititionImplBrowsertest() {
    if (GetParam() == NetworkServiceState::kEnabled)
      feature_list_.InitAndEnableFeature(features::kNetworkService);
  }
  ~StoragePartititionImplBrowsertest() override {}

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Make sure that the NetworkContext returned by a StoragePartition works, both
// with the network service enabled and with it disabled, when one is created
// that wraps the URLRequestContext created by the BrowserContext.
IN_PROC_BROWSER_TEST_P(StoragePartititionImplBrowsertest, NetworkContext) {
  ASSERT_TRUE(embedded_test_server()->Start());

  network::mojom::URLLoaderFactoryPtr loader_factory;
  BrowserContext::GetDefaultStoragePartition(
      shell()->web_contents()->GetBrowserContext())
      ->GetNetworkContext()
      ->CreateURLLoaderFactory(mojo::MakeRequest(&loader_factory), 0);

  network::ResourceRequest request;
  TestURLLoaderClient client;
  request.url = embedded_test_server()->GetURL("/set-header?foo: bar");
  request.method = "GET";
  network::mojom::URLLoaderPtr loader;
  loader_factory->CreateLoaderAndStart(
      mojo::MakeRequest(&loader), 2, 1, network::mojom::kURLLoadOptionNone,
      request, client.CreateInterfacePtr(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  // Just wait until headers are received - if the right headers are received,
  // no need to read the body.
  client.RunUntilResponseBodyArrived();
  ASSERT_TRUE(client.response_head().headers);
  EXPECT_EQ(200, client.response_head().headers->response_code());

  std::string foo_header_value;
  ASSERT_TRUE(client.response_head().headers->GetNormalizedHeader(
      "foo", &foo_header_value));
  EXPECT_EQ("bar", foo_header_value);
}

// NetworkServiceState::kEnabled currently DCHECKs on Android, as Android isn't
// expected to create extra processes.
#if defined(OS_ANDROID)
INSTANTIATE_TEST_CASE_P(
    /* No test prefix */,
    StoragePartititionImplBrowsertest,
    ::testing::Values(NetworkServiceState::kDisabled));
#else  // !defined(OS_ANDROID)
INSTANTIATE_TEST_CASE_P(
    /* No test prefix */,
    StoragePartititionImplBrowsertest,
    ::testing::Values(NetworkServiceState::kDisabled,
                      NetworkServiceState::kEnabled));
#endif

}  // namespace content
