// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/system_network_context_manager.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/network_service.mojom.h"
#include "content/public/common/resource_response.h"
#include "content/public/common/resource_response_info.h"
#include "content/public/common/url_loader.mojom.h"
#include "content/public/common/url_loader_factory.mojom.h"
#include "content/public/test/test_url_loader_client.h"
#include "mojo/common/data_pipe_utils.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"

namespace {

enum class NetworkServiceState {
  kDisabled,
  kEnabled,
};

class SystemNetworkContextManagerTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<NetworkServiceState> {
 public:
  SystemNetworkContextManagerTest() {
    EXPECT_TRUE(embedded_test_server()->Start());
  }

  ~SystemNetworkContextManagerTest() override {}

  void SetUpInProcessBrowserTestFixture() override {
    feature_list_.InitAndEnableFeature(features::kNetworkService);
  }

  void SetUpOnMainThread() override {
    SystemNetworkContextManager::Context()->CreateURLLoaderFactory(
        MakeRequest(&loader_factory_), 0);
  }

  content::mojom::URLLoaderFactory* loader_factory() {
    return loader_factory_.get();
  }

 private:
  content::mojom::URLLoaderFactoryPtr loader_factory_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(SystemNetworkContextManagerTest, BasicRequest) {
  content::mojom::URLLoaderAssociatedPtr loader;
  content::ResourceRequest request;
  content::TestURLLoaderClient client;
  request.url = embedded_test_server()->GetURL("/echo");
  request.method = "GET";
  request.request_initiator = url::Origin();
  loader_factory()->CreateLoaderAndStart(
      mojo::MakeRequest(&loader), 2, 1, content::mojom::kURLLoadOptionNone,
      request, client.CreateInterfacePtr(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  client.RunUntilResponseReceived();
  ASSERT_TRUE(client.response_head().headers);
  EXPECT_EQ(200, client.response_head().headers->response_code());

  client.RunUntilResponseBodyArrived();
  // TODO(mmenke):  Is blocking the UI Thread while reading the response really
  // the best way to test requests in a browser test?
  std::string response_body;
  EXPECT_TRUE(mojo::common::BlockingCopyToString(client.response_body_release(),
                                                 &response_body));
  EXPECT_EQ("Echo", response_body);

  client.RunUntilComplete();
  EXPECT_EQ(net::OK, client.completion_status().error_code);
}

INSTANTIATE_TEST_CASE_P(
    /* no prefix */,
    SystemNetworkContextManagerTest,
    ::testing::Values(NetworkServiceState::kDisabled,
                      NetworkServiceState::kEnabled));

}  // namespace
