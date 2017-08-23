// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/network_service.mojom.h"
#include "content/public/common/resource_response.h"
#include "content/public/common/resource_response_info.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_loader.mojom.h"
#include "content/public/common/url_loader_factory.mojom.h"
#include "content/public/test/test_url_loader_client.h"
#include "mojo/common/data_pipe_utils.h"
#include "net/base/filename_util.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

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
  // Backing storage types that can used for various things (HTTP cache,
  // cookies, etc).
  enum class StorageType {
    kNone,
    kMemory,
    kDisk,
  };

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
        network_context_ =
            g_browser_process->system_network_context_manager()->GetContext();
        break;
      }
      case NetworkContextType::kProfile: {
        network_context_ = content::BrowserContext::GetDefaultStoragePartition(
                               browser()->profile())
                               ->GetNetworkContext();
        break;
      }
      case NetworkContextType::kIncognitoProfile: {
        Browser* incognito = CreateIncognitoBrowser();
        network_context_ = content::BrowserContext::GetDefaultStoragePartition(
                               incognito->profile())
                               ->GetNetworkContext();
        break;
      }
    }
    network_context_->CreateURLLoaderFactory(MakeRequest(&loader_factory_), 0);
  }

  content::mojom::URLLoaderFactory* loader_factory() const {
    return loader_factory_.get();
  }

  content::mojom::NetworkContext* network_context() const {
    return network_context_;
  }

  StorageType GetHttpCacheType() const {
    switch (GetParam().network_context_type) {
      case NetworkContextType::kSystem:
        return StorageType::kNone;
      case NetworkContextType::kProfile:
        return StorageType::kDisk;
      case NetworkContextType::kIncognitoProfile:
        return StorageType::kMemory;
    }
    NOTREACHED();
    return StorageType::kNone;
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

// Make sure a cache is used when expected.
IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationBrowserTest, Cache) {
  // Make a request whose response should be cached.
  content::mojom::URLLoaderPtr loader;
  content::ResourceRequest request;
  content::TestURLLoaderClient client;
  request.url = embedded_test_server()->GetURL("/cachetime");
  request.method = "GET";
  loader_factory()->CreateLoaderAndStart(
      mojo::MakeRequest(&loader), 2, 1, content::mojom::kURLLoadOptionNone,
      request, client.CreateInterfacePtr(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  client.RunUntilResponseReceived();
  ASSERT_TRUE(client.response_head().headers);
  EXPECT_EQ(200, client.response_head().headers->response_code());
  client.RunUntilResponseBodyArrived();
  std::string response_body;
  EXPECT_TRUE(mojo::common::BlockingCopyToString(client.response_body_release(),
                                                 &response_body));
  EXPECT_GE(response_body.size(), 0u);
  client.RunUntilComplete();
  EXPECT_EQ(net::OK, client.completion_status().error_code);
  EXPECT_FALSE(client.completion_status().exists_in_cache);

  // Stop the server.
  ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());

  // Make the request again, and make sure it's cached or not, according to
  // expectations. Reuse the content::ResourceRequest, but nothing else.
  content::mojom::URLLoaderPtr loader2;
  content::TestURLLoaderClient client2;
  loader_factory()->CreateLoaderAndStart(
      mojo::MakeRequest(&loader2), 3, 2, content::mojom::kURLLoadOptionNone,
      request, client2.CreateInterfacePtr(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  if (GetHttpCacheType() == StorageType::kNone) {
    client2.RunUntilComplete();
    // If there's no cache, and not server running, the request should fail.
    EXPECT_EQ(net::ERR_CONNECTION_REFUSED,
              client2.completion_status().error_code);
  } else {
    // Otherwise, the request should succeed, and return the same result as
    // before.
    client2.RunUntilResponseReceived();
    ASSERT_TRUE(client2.response_head().headers);
    EXPECT_EQ(200, client2.response_head().headers->response_code());
    client2.RunUntilResponseBodyArrived();
    std::string response_body2;
    EXPECT_TRUE(mojo::common::BlockingCopyToString(
        client2.response_body_release(), &response_body2));
    EXPECT_EQ(response_body, response_body2);
    client2.RunUntilComplete();
    EXPECT_EQ(net::OK, client2.completion_status().error_code);
    EXPECT_TRUE(client2.completion_status().exists_in_cache);
  }
}

// Make sure an on-disk cache is used when expected. PRE_DiskCache populates the
// cache. DiskCache then makes sure the cache entry is still there (Or not) as
// expected.
IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationBrowserTest, PRE_DiskCache) {
  // Save test URL to disk, so it can be used in the next test (Test server uses
  // a random port, so need to know the port to try and retrieve it from the
  // cache in the next test). The profile directory is preserved between the
  // PRE_DiskCache and DiskCache run, so can just keep a file there.
  GURL test_url = embedded_test_server()->GetURL("/echoheadercache?foo");
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  base::FilePath save_url_file_path = browser()->profile()->GetPath().Append(
      FILE_PATH_LITERAL("url_for_test.txt"));
  ASSERT_EQ(static_cast<int>(test_url.spec().length()),
            base::WriteFile(save_url_file_path, test_url.spec().c_str(),
                            test_url.spec().length()));

  // Make a request whose response should be cached.
  content::mojom::URLLoaderPtr loader;
  content::ResourceRequest request;
  content::TestURLLoaderClient client;
  request.url = test_url;
  request.method = "GET";
  request.headers = "foo: foopity foo\r\n\r\n";
  loader_factory()->CreateLoaderAndStart(
      mojo::MakeRequest(&loader), 2, 1, content::mojom::kURLLoadOptionNone,
      request, client.CreateInterfacePtr(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  client.RunUntilResponseReceived();
  ASSERT_TRUE(client.response_head().headers);
  EXPECT_EQ(200, client.response_head().headers->response_code());
  client.RunUntilResponseBodyArrived();
  std::string response_body;
  EXPECT_TRUE(mojo::common::BlockingCopyToString(client.response_body_release(),
                                                 &response_body));
  EXPECT_EQ("foopity foo", response_body);
  client.RunUntilComplete();
  EXPECT_EQ(net::OK, client.completion_status().error_code);
  EXPECT_FALSE(client.completion_status().exists_in_cache);
}

// Check if the URL loaded in PRE_DiskCache is still in the cache, across a
// browser restart.
IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationBrowserTest, DiskCache) {
  // Load URL from the above test body to disk.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  base::FilePath save_url_file_path = browser()->profile()->GetPath().Append(
      FILE_PATH_LITERAL("url_for_test.txt"));
  std::string test_url_string;
  ASSERT_TRUE(ReadFileToString(save_url_file_path, &test_url_string));
  GURL test_url = GURL(content::kChromeUINetworkViewCacheURL + test_url_string);
  ASSERT_TRUE(test_url.is_valid()) << test_url_string;

  content::TestURLLoaderClient client;
  // Read from the cache directly, as the test server may theoretically have
  // been restarted on the same port by another test.
  network_context()->HandleViewCacheRequest(test_url,
                                            client.CreateInterfacePtr());

  // The request should succeed, whether the response was cached or not.
  client.RunUntilResponseReceived();
  ASSERT_TRUE(client.response_head().headers);
  EXPECT_EQ(200, client.response_head().headers->response_code());
  client.RunUntilResponseBodyArrived();
  std::string response_body;
  EXPECT_TRUE(mojo::common::BlockingCopyToString(client.response_body_release(),
                                                 &response_body));
  client.RunUntilComplete();
  EXPECT_EQ(net::OK, client.completion_status().error_code);

  // The response body from the above test should only appear in the view-cache
  // result if there is an on-disk cache.
  if (GetHttpCacheType() != StorageType::kDisk) {
    EXPECT_EQ(response_body.find("foopity foo"), std::string::npos);
  } else {
    EXPECT_NE(response_body.find("foopity foo"), std::string::npos);
  }
}

class NetworkContextConfigurationFixedPortBrowserTest
    : public NetworkContextConfigurationBrowserTest {
 public:
  NetworkContextConfigurationFixedPortBrowserTest() {}
  ~NetworkContextConfigurationFixedPortBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kTestingFixedHttpPort,
        base::StringPrintf("%u", embedded_test_server()->port()));
    LOG(WARNING) << base::StringPrintf("%u", embedded_test_server()->port());
  }

 private:
};

// Test that the command line switch makes it to the network service and is
// respected.
IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationFixedPortBrowserTest,
                       TestingFixedPort) {
  content::mojom::URLLoaderPtr loader;
  content::ResourceRequest request;
  content::TestURLLoaderClient client;
  // This URL does not use the port the embedded test server is using. The
  // command line switch should make it result in the request being directed to
  // the test server anyways.
  request.url = GURL("http://127.0.0.1/echo");
  loader_factory()->CreateLoaderAndStart(
      mojo::MakeRequest(&loader), 0, 0, content::mojom::kURLLoadOptionNone,
      request, client.CreateInterfacePtr(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  client.RunUntilResponseReceived();
  ASSERT_TRUE(client.response_head().headers);
  EXPECT_EQ(200, client.response_head().headers->response_code());

  client.RunUntilResponseBodyArrived();
  std::string response_body;
  EXPECT_TRUE(mojo::common::BlockingCopyToString(client.response_body_release(),
                                                 &response_body));
  EXPECT_EQ("Echo", response_body);

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

INSTANTIATE_TEST_CASE_P(
    SystemNetworkContext,
    NetworkContextConfigurationFixedPortBrowserTest,
    ::testing::Values(TestCase({NetworkServiceState::kDisabled,
                                NetworkContextType::kSystem}),
                      TestCase({NetworkServiceState::kEnabled,
                                NetworkContextType::kSystem})));

INSTANTIATE_TEST_CASE_P(
    ProfileMainNetworkContext,
    NetworkContextConfigurationFixedPortBrowserTest,
    ::testing::Values(TestCase({NetworkServiceState::kDisabled,
                                NetworkContextType::kProfile}),
                      TestCase({NetworkServiceState::kEnabled,
                                NetworkContextType::kProfile})));

INSTANTIATE_TEST_CASE_P(
    IncognitoProfileMainNetworkContext,
    NetworkContextConfigurationFixedPortBrowserTest,
    ::testing::Values(TestCase({NetworkServiceState::kDisabled,
                                NetworkContextType::kIncognitoProfile}),
                      TestCase({NetworkServiceState::kEnabled,
                                NetworkContextType::kIncognitoProfile})));

}  // namespace
