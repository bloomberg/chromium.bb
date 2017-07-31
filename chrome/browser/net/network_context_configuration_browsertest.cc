// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
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
#include "net/base/filename_util.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"

namespace {

enum class NetworkServiceState {
  kDisabled,
  kEnabled,
};

enum class NetworkContextType {
  kSystem,
  kProfile,
  kIncognitoProfile,
};

struct TestCase {
  NetworkServiceState network_service_state;
  NetworkContextType network_context_type;
};

// Tests the system, profile, and incognito profile NetworkContexts.
class NetworkContextConfigurationBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<TestCase> {
 public:
  NetworkContextConfigurationBrowserTest() {
    EXPECT_TRUE(embedded_test_server()->Start());
  }

  ~NetworkContextConfigurationBrowserTest() override {}

  void SetUpInProcessBrowserTestFixture() override {
    if (GetParam().network_service_state == NetworkServiceState::kEnabled)
      feature_list_.InitAndEnableFeature(features::kNetworkService);
  }

  void SetUpOnMainThread() override {
    switch (GetParam().network_context_type) {
      case NetworkContextType::kSystem: {
        network_context_ = SystemNetworkContextManager::Context();
        break;
      }
      case NetworkContextType::kProfile: {
        network_context_ = ProfileNetworkContextServiceFactory::GetInstance()
                               ->GetForContext(browser()->profile())
                               ->MainContext();
        break;
      }
      case NetworkContextType::kIncognitoProfile: {
        Browser* incognito = CreateIncognitoBrowser();
        network_context_ = ProfileNetworkContextServiceFactory::GetInstance()
                               ->GetForContext(incognito->profile())
                               ->MainContext();
        break;
      }
    }
    network_context_->CreateURLLoaderFactory(MakeRequest(&loader_factory_), 0);
  }

  content::mojom::URLLoaderFactory* loader_factory() {
    return loader_factory_.get();
  }

 private:
  content::mojom::NetworkContext* network_context_ = nullptr;
  content::mojom::URLLoaderFactoryPtr loader_factory_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationBrowserTest, BasicRequest) {
  content::mojom::URLLoaderPtr loader;
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

IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationBrowserTest, DataURL) {
  content::mojom::URLLoaderPtr loader;
  content::ResourceRequest request;
  content::TestURLLoaderClient client;
  request.url = GURL("data:text/plain,foo");
  request.method = "GET";
  request.request_initiator = url::Origin();
  loader_factory()->CreateLoaderAndStart(
      mojo::MakeRequest(&loader), 2, 1, content::mojom::kURLLoadOptionNone,
      request, client.CreateInterfacePtr(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  client.RunUntilResponseReceived();
  // Data URLs don't have headers.
  EXPECT_FALSE(client.response_head().headers);
  EXPECT_EQ("text/plain", client.response_head().mime_type);

  client.RunUntilResponseBodyArrived();
  std::string response_body;
  EXPECT_TRUE(mojo::common::BlockingCopyToString(client.response_body_release(),
                                                 &response_body));
  EXPECT_EQ("foo", response_body);

  client.RunUntilComplete();
  EXPECT_EQ(net::OK, client.completion_status().error_code);
}

IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationBrowserTest, FileURL) {
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  base::ScopedTempDir temp_dir_;
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  base::FilePath file_path;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &file_path));
  const char kFileContents[] = "This file intentionally left empty.";
  ASSERT_EQ(static_cast<int>(strlen(kFileContents)),
            base::WriteFile(file_path, kFileContents, strlen(kFileContents)));

  content::mojom::URLLoaderPtr loader;
  content::ResourceRequest request;
  content::TestURLLoaderClient client;
  request.url = net::FilePathToFileURL(file_path);
  request.method = "GET";
  request.request_initiator = url::Origin();
  loader_factory()->CreateLoaderAndStart(
      mojo::MakeRequest(&loader), 2, 1, content::mojom::kURLLoadOptionNone,
      request, client.CreateInterfacePtr(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  client.RunUntilResponseReceived();
  // File URLs don't have headers.
  EXPECT_FALSE(client.response_head().headers);

  client.RunUntilResponseBodyArrived();
  std::string response_body;
  EXPECT_TRUE(mojo::common::BlockingCopyToString(client.response_body_release(),
                                                 &response_body));
  EXPECT_EQ(kFileContents, response_body);

  client.RunUntilComplete();
  EXPECT_EQ(net::OK, client.completion_status().error_code);
}

INSTANTIATE_TEST_CASE_P(
    SystemNetworkContext,
    NetworkContextConfigurationBrowserTest,
    ::testing::Values(TestCase({NetworkServiceState::kDisabled,
                                NetworkContextType::kSystem}),
                      TestCase({NetworkServiceState::kEnabled,
                                NetworkContextType::kSystem})));

INSTANTIATE_TEST_CASE_P(
    ProfileMainNetworkContext,
    NetworkContextConfigurationBrowserTest,
    ::testing::Values(TestCase({NetworkServiceState::kDisabled,
                                NetworkContextType::kProfile}),
                      TestCase({NetworkServiceState::kEnabled,
                                NetworkContextType::kProfile})));

INSTANTIATE_TEST_CASE_P(
    IncognitoProfileMainNetworkContext,
    NetworkContextConfigurationBrowserTest,
    ::testing::Values(TestCase({NetworkServiceState::kDisabled,
                                NetworkContextType::kIncognitoProfile}),
                      TestCase({NetworkServiceState::kEnabled,
                                NetworkContextType::kIncognitoProfile})));

}  // namespace
