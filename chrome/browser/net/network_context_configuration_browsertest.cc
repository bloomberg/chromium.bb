// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/network_service.mojom.h"
#include "content/public/common/resource_response.h"
#include "content/public/common/resource_response_info.h"
#include "content/public/common/simple_url_loader.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_loader.mojom.h"
#include "content/public/common/url_loader_factory.mojom.h"
#include "content/public/test/simple_url_loader_test_helper.h"
#include "content/public/test/test_url_loader_client.h"
#include "mojo/common/data_pipe_utils.h"
#include "net/base/filename_util.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
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
        SystemNetworkContextManager* system_network_context_manager =
            g_browser_process->system_network_context_manager();
        network_context_ = system_network_context_manager->GetContext();
        loader_factory_ = system_network_context_manager->GetURLLoaderFactory();
        break;
      }
      case NetworkContextType::kProfile: {
        content::StoragePartition* storage_partition =
            content::BrowserContext::GetDefaultStoragePartition(
                browser()->profile());
        network_context_ = storage_partition->GetNetworkContext();
        loader_factory_ =
            storage_partition->GetURLLoaderFactoryForBrowserProcess();
        break;
      }
      case NetworkContextType::kIncognitoProfile: {
        Browser* incognito = CreateIncognitoBrowser();
        content::StoragePartition* storage_partition =
            content::BrowserContext::GetDefaultStoragePartition(
                incognito->profile());
        network_context_ = storage_partition->GetNetworkContext();
        loader_factory_ =
            storage_partition->GetURLLoaderFactoryForBrowserProcess();
        break;
      }
    }
  }

  // Returns, as a string, a PAC script that will use the EmbeddedTestServer as
  // a proxy.
  std::string GetPacScript() const {
    return base::StringPrintf(
        "function FindProxyForURL(url, host){ return 'PROXY %s;'; }",
        net::HostPortPair::FromURL(embedded_test_server()->base_url())
            .ToString()
            .c_str());
  }

  content::mojom::URLLoaderFactory* loader_factory() const {
    return loader_factory_;
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

  // Sets the proxy preference on a PrefService based on the NetworkContextType,
  // and waits for it to be applied.
  void SetProxyPref(const net::HostPortPair& host_port_pair) {
    // Get the correct PrefService.
    PrefService* pref_service = nullptr;
    switch (GetParam().network_context_type) {
      case NetworkContextType::kSystem:
        pref_service = g_browser_process->local_state();
        break;
      case NetworkContextType::kProfile:
        pref_service = browser()->profile()->GetPrefs();
        break;
      case NetworkContextType::kIncognitoProfile:
        // Incognito uses the non-incognito prefs.
        pref_service =
            browser()->profile()->GetOffTheRecordProfile()->GetPrefs();
        break;
    }

    pref_service->Set(proxy_config::prefs::kProxy,
                      *ProxyConfigDictionary::CreateFixedServers(
                          host_port_pair.ToString(), std::string()));

    // Wait for the new ProxyConfig to be passed over the pipe. Needed because
    // Mojo doesn't guarantee ordering of events on different Mojo pipes, and
    // requests are sent on a separate pipe from ProxyConfigs.
    switch (GetParam().network_context_type) {
      case NetworkContextType::kSystem:
        g_browser_process->system_network_context_manager()
            ->FlushProxyConfigMonitorForTesting();
        break;
      case NetworkContextType::kProfile:
        ProfileNetworkContextServiceFactory::GetForContext(browser()->profile())
            ->FlushProxyConfigMonitorForTesting();
        break;
      case NetworkContextType::kIncognitoProfile:
        ProfileNetworkContextServiceFactory::GetForContext(
            browser()->profile()->GetOffTheRecordProfile())
            ->FlushProxyConfigMonitorForTesting();
        break;
    }
  }

  // Sends a request and expects it to be handled by embedded_test_server()
  // acting as a proxy;
  void TestProxyConfigured() {
    std::unique_ptr<content::ResourceRequest> request =
        std::make_unique<content::ResourceRequest>();
    // This URL should be directed to the test server because of the proxy.
    request->url = GURL("http://jabberwocky.test:1872/echo");

    content::SimpleURLLoaderTestHelper simple_loader_helper;
    std::unique_ptr<content::SimpleURLLoader> simple_loader =
        content::SimpleURLLoader::Create(std::move(request),
                                         TRAFFIC_ANNOTATION_FOR_TESTS);

    simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        loader_factory(), simple_loader_helper.GetCallback());
    simple_loader_helper.WaitForCallback();

    EXPECT_EQ(net::OK, simple_loader->NetError());
    ASSERT_TRUE(simple_loader_helper.response_body());
    EXPECT_EQ(*simple_loader_helper.response_body(), "Echo");
  }

 private:
  content::mojom::NetworkContext* network_context_ = nullptr;
  content::mojom::URLLoaderFactory* loader_factory_ = nullptr;
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(NetworkContextConfigurationBrowserTest);
};

IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationBrowserTest, BasicRequest) {
  std::unique_ptr<content::ResourceRequest> request =
      std::make_unique<content::ResourceRequest>();
  request->url = embedded_test_server()->GetURL("/echo");
  content::SimpleURLLoaderTestHelper simple_loader_helper;
  std::unique_ptr<content::SimpleURLLoader> simple_loader =
      content::SimpleURLLoader::Create(std::move(request),
                                       TRAFFIC_ANNOTATION_FOR_TESTS);

  simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory(), simple_loader_helper.GetCallback());
  simple_loader_helper.WaitForCallback();

  ASSERT_TRUE(simple_loader->ResponseInfo());
  ASSERT_TRUE(simple_loader->ResponseInfo()->headers);
  EXPECT_EQ(200, simple_loader->ResponseInfo()->headers->response_code());
  ASSERT_TRUE(simple_loader_helper.response_body());
  EXPECT_EQ("Echo", *simple_loader_helper.response_body());
}

IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationBrowserTest, DataURL) {
  std::unique_ptr<content::ResourceRequest> request =
      std::make_unique<content::ResourceRequest>();
  request->url = GURL("data:text/plain,foo");
  content::SimpleURLLoaderTestHelper simple_loader_helper;
  std::unique_ptr<content::SimpleURLLoader> simple_loader =
      content::SimpleURLLoader::Create(std::move(request),
                                       TRAFFIC_ANNOTATION_FOR_TESTS);

  simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory(), simple_loader_helper.GetCallback());
  simple_loader_helper.WaitForCallback();

  ASSERT_TRUE(simple_loader->ResponseInfo());
  // Data URLs don't have headers.
  EXPECT_FALSE(simple_loader->ResponseInfo()->headers);
  EXPECT_EQ("text/plain", simple_loader->ResponseInfo()->mime_type);
  ASSERT_TRUE(simple_loader_helper.response_body());
  EXPECT_EQ("foo", *simple_loader_helper.response_body());
}

IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationBrowserTest, FileURL) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir_;
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  base::FilePath file_path;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &file_path));
  const char kFileContents[] = "This file intentionally left empty.";
  ASSERT_EQ(static_cast<int>(strlen(kFileContents)),
            base::WriteFile(file_path, kFileContents, strlen(kFileContents)));

  std::unique_ptr<content::ResourceRequest> request =
      std::make_unique<content::ResourceRequest>();
  request->url = net::FilePathToFileURL(file_path);
  content::SimpleURLLoaderTestHelper simple_loader_helper;
  std::unique_ptr<content::SimpleURLLoader> simple_loader =
      content::SimpleURLLoader::Create(std::move(request),
                                       TRAFFIC_ANNOTATION_FOR_TESTS);

  simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory(), simple_loader_helper.GetCallback());
  simple_loader_helper.WaitForCallback();

  ASSERT_TRUE(simple_loader->ResponseInfo());
  // File URLs don't have headers.
  EXPECT_FALSE(simple_loader->ResponseInfo()->headers);
  ASSERT_TRUE(simple_loader_helper.response_body());
  EXPECT_EQ(kFileContents, *simple_loader_helper.response_body());
}

// Make sure a cache is used when expected.
IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationBrowserTest, Cache) {
  // Make a request whose response should be cached.
  GURL request_url = embedded_test_server()->GetURL("/cachetime");
  std::unique_ptr<content::ResourceRequest> request =
      std::make_unique<content::ResourceRequest>();
  request->url = request_url;
  content::SimpleURLLoaderTestHelper simple_loader_helper;
  std::unique_ptr<content::SimpleURLLoader> simple_loader =
      content::SimpleURLLoader::Create(std::move(request),
                                       TRAFFIC_ANNOTATION_FOR_TESTS);

  simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory(), simple_loader_helper.GetCallback());
  simple_loader_helper.WaitForCallback();

  ASSERT_TRUE(simple_loader_helper.response_body());
  EXPECT_GT(simple_loader_helper.response_body()->size(), 0u);

  // Stop the server.
  ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());

  // Make the request again, and make sure it's cached or not, according to
  // expectations. Reuse the content::ResourceRequest, but nothing else.
  std::unique_ptr<content::ResourceRequest> request2 =
      std::make_unique<content::ResourceRequest>();
  request2->url = request_url;
  content::SimpleURLLoaderTestHelper simple_loader_helper2;
  std::unique_ptr<content::SimpleURLLoader> simple_loader2 =
      content::SimpleURLLoader::Create(std::move(request2),
                                       TRAFFIC_ANNOTATION_FOR_TESTS);
  simple_loader2->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory(), simple_loader_helper2.GetCallback());
  simple_loader_helper2.WaitForCallback();
  if (GetHttpCacheType() == StorageType::kNone) {
    // If there's no cache, and not server running, the request should have
    // failed.
    EXPECT_FALSE(simple_loader_helper2.response_body());
    EXPECT_EQ(net::ERR_CONNECTION_REFUSED, simple_loader2->NetError());
  } else {
    // Otherwise, the request should have succeeded, and returned the same
    // result as before.
    ASSERT_TRUE(simple_loader_helper2.response_body());
    EXPECT_EQ(*simple_loader_helper.response_body(),
              *simple_loader_helper2.response_body());
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
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath save_url_file_path = browser()->profile()->GetPath().Append(
      FILE_PATH_LITERAL("url_for_test.txt"));
  ASSERT_EQ(static_cast<int>(test_url.spec().length()),
            base::WriteFile(save_url_file_path, test_url.spec().c_str(),
                            test_url.spec().length()));

  // Make a request whose response should be cached.
  std::unique_ptr<content::ResourceRequest> request =
      std::make_unique<content::ResourceRequest>();
  request->url = test_url;
  request->headers.SetHeader("foo", "foopity foo");
  content::SimpleURLLoaderTestHelper simple_loader_helper;
  std::unique_ptr<content::SimpleURLLoader> simple_loader =
      content::SimpleURLLoader::Create(std::move(request),
                                       TRAFFIC_ANNOTATION_FOR_TESTS);
  simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory(), simple_loader_helper.GetCallback());
  simple_loader_helper.WaitForCallback();

  EXPECT_EQ(net::OK, simple_loader->NetError());
  ASSERT_TRUE(simple_loader_helper.response_body());
  EXPECT_EQ(*simple_loader_helper.response_body(), "foopity foo");
}

// Check if the URL loaded in PRE_DiskCache is still in the cache, across a
// browser restart.
IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationBrowserTest, DiskCache) {
  // Load URL from the above test body to disk.
  base::ScopedAllowBlockingForTesting allow_blocking;
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

IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationBrowserTest, ProxyConfig) {
  SetProxyPref(embedded_test_server()->host_port_pair());
  TestProxyConfigured();
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
};

// Test that the command line switch makes it to the network service and is
// respected.
IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationFixedPortBrowserTest,
                       TestingFixedPort) {
  std::unique_ptr<content::ResourceRequest> request =
      std::make_unique<content::ResourceRequest>();
  // This URL does not use the port the embedded test server is using. The
  // command line switch should make it result in the request being directed to
  // the test server anyways.
  request->url = GURL("http://127.0.0.1/echo");
  content::SimpleURLLoaderTestHelper simple_loader_helper;
  std::unique_ptr<content::SimpleURLLoader> simple_loader =
      content::SimpleURLLoader::Create(std::move(request),
                                       TRAFFIC_ANNOTATION_FOR_TESTS);

  simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory(), simple_loader_helper.GetCallback());
  simple_loader_helper.WaitForCallback();

  EXPECT_EQ(net::OK, simple_loader->NetError());
  ASSERT_TRUE(simple_loader_helper.response_body());
  EXPECT_EQ(*simple_loader_helper.response_body(), "Echo");
}

class NetworkContextConfigurationProxyOnStartBrowserTest
    : public NetworkContextConfigurationBrowserTest {
 public:
  NetworkContextConfigurationProxyOnStartBrowserTest() {}
  ~NetworkContextConfigurationProxyOnStartBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kProxyServer,
        embedded_test_server()->host_port_pair().ToString());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkContextConfigurationProxyOnStartBrowserTest);
};

// Test that when there's a proxy configuration at startup, the initial requests
// use that configuration.
IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationProxyOnStartBrowserTest,
                       TestInitialProxyConfig) {
  TestProxyConfigured();
}

// Make sure the system URLRequestContext can handle fetching PAC scripts from
// http URLs.
class NetworkContextConfigurationHttpPacBrowserTest
    : public NetworkContextConfigurationBrowserTest {
 public:
  NetworkContextConfigurationHttpPacBrowserTest()
      : pac_test_server_(net::test_server::EmbeddedTestServer::TYPE_HTTP) {}

  ~NetworkContextConfigurationHttpPacBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    pac_test_server_.RegisterRequestHandler(base::Bind(
        &NetworkContextConfigurationHttpPacBrowserTest::HandlePacRequest,
        GetPacScript()));
    EXPECT_TRUE(pac_test_server_.Start());

    command_line->AppendSwitchASCII(switches::kProxyPacUrl,
                                    pac_test_server_.base_url().spec().c_str());
  }

  static std::unique_ptr<net::test_server::HttpResponse> HandlePacRequest(
      const std::string& pac_script,
      const net::test_server::HttpRequest& request) {
    std::unique_ptr<net::test_server::BasicHttpResponse> response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_content(pac_script);
    return response;
  }

 private:
  net::test_server::EmbeddedTestServer pac_test_server_;

  DISALLOW_COPY_AND_ASSIGN(NetworkContextConfigurationHttpPacBrowserTest);
};

IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationHttpPacBrowserTest, HttpPac) {
  TestProxyConfigured();
}

// Make sure the system URLRequestContext can handle fetching PAC scripts from
// file URLs.
class NetworkContextConfigurationFilePacBrowserTest
    : public NetworkContextConfigurationBrowserTest {
 public:
  NetworkContextConfigurationFilePacBrowserTest() {}

  ~NetworkContextConfigurationFilePacBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    const char kPacFileName[] = "foo.pac";

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath pac_file_path =
        temp_dir_.GetPath().AppendASCII(kPacFileName);

    std::string pac_script = GetPacScript();
    ASSERT_EQ(
        static_cast<int>(pac_script.size()),
        base::WriteFile(pac_file_path, pac_script.c_str(), pac_script.size()));

    command_line->AppendSwitchASCII(
        switches::kProxyPacUrl, net::FilePathToFileURL(pac_file_path).spec());
  }

 private:
  base::ScopedTempDir temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(NetworkContextConfigurationFilePacBrowserTest);
};

IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationFilePacBrowserTest, FilePac) {
  TestProxyConfigured();
}

// Make sure the system URLRequestContext can handle fetching PAC scripts from
// data URLs.
class NetworkContextConfigurationDataPacBrowserTest
    : public NetworkContextConfigurationBrowserTest {
 public:
  NetworkContextConfigurationDataPacBrowserTest() {}
  ~NetworkContextConfigurationDataPacBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    std::string contents;
    // Read in kPACScript contents.
    command_line->AppendSwitchASCII(switches::kProxyPacUrl,
                                    "data:," + GetPacScript());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkContextConfigurationDataPacBrowserTest);
};

IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationDataPacBrowserTest, DataPac) {
  TestProxyConfigured();
}

// Make sure the system URLRequestContext can handle fetching PAC scripts from
// ftp URLs. Unlike the other PAC tests, this test uses a PAC script that
// results in an error, since the spawned test server is designed so that it can
// run remotely (So can't just write a script to a local file and have the
// server serve it).
class NetworkContextConfigurationFtpPacBrowserTest
    : public NetworkContextConfigurationBrowserTest {
 public:
  NetworkContextConfigurationFtpPacBrowserTest()
      : ftp_server_(net::SpawnedTestServer::TYPE_FTP,
                    base::FilePath(FILE_PATH_LITERAL("chrome/test/data"))) {
    EXPECT_TRUE(ftp_server_.Start());
  }
  ~NetworkContextConfigurationFtpPacBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kProxyPacUrl,
        ftp_server_.GetURL("bad_server.pac").spec().c_str());
  }

 private:
  net::SpawnedTestServer ftp_server_;

  DISALLOW_COPY_AND_ASSIGN(NetworkContextConfigurationFtpPacBrowserTest);
};

IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationFtpPacBrowserTest, FtpPac) {
  std::unique_ptr<content::ResourceRequest> request =
      std::make_unique<content::ResourceRequest>();
  // This URL should be directed to the test server because of the proxy.
  request->url = GURL("http://jabberwocky.test:1872/echo");

  content::SimpleURLLoaderTestHelper simple_loader_helper;
  std::unique_ptr<content::SimpleURLLoader> simple_loader =
      content::SimpleURLLoader::Create(std::move(request),
                                       TRAFFIC_ANNOTATION_FOR_TESTS);

  simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory(), simple_loader_helper.GetCallback());
  simple_loader_helper.WaitForCallback();

  EXPECT_EQ(net::ERR_PROXY_CONNECTION_FAILED, simple_loader->NetError());
}

// Instiates tests with a prefix indicating which NetworkContext is being
// tested, and a suffix of "/0" if the network service is disabled and "/1" if
// it's enabled.
#define INSTANTIATE_TEST_CASES_FOR_TEST_FIXTURE(TestFixture)               \
  INSTANTIATE_TEST_CASE_P(                                                 \
      SystemNetworkContext, TestFixture,                                   \
      ::testing::Values(TestCase({NetworkServiceState::kDisabled,          \
                                  NetworkContextType::kSystem}),           \
                        TestCase({NetworkServiceState::kEnabled,           \
                                  NetworkContextType::kSystem})));         \
                                                                           \
  INSTANTIATE_TEST_CASE_P(                                                 \
      ProfileMainNetworkContext, TestFixture,                              \
      ::testing::Values(TestCase({NetworkServiceState::kDisabled,          \
                                  NetworkContextType::kProfile}),          \
                        TestCase({NetworkServiceState::kEnabled,           \
                                  NetworkContextType::kProfile})));        \
                                                                           \
  INSTANTIATE_TEST_CASE_P(                                                 \
      IncognitoProfileMainNetworkContext, TestFixture,                     \
      ::testing::Values(TestCase({NetworkServiceState::kDisabled,          \
                                  NetworkContextType::kIncognitoProfile}), \
                        TestCase({NetworkServiceState::kEnabled,           \
                                  NetworkContextType::kIncognitoProfile})))

INSTANTIATE_TEST_CASES_FOR_TEST_FIXTURE(NetworkContextConfigurationBrowserTest);
INSTANTIATE_TEST_CASES_FOR_TEST_FIXTURE(
    NetworkContextConfigurationFixedPortBrowserTest);
INSTANTIATE_TEST_CASES_FOR_TEST_FIXTURE(
    NetworkContextConfigurationProxyOnStartBrowserTest);
INSTANTIATE_TEST_CASES_FOR_TEST_FIXTURE(
    NetworkContextConfigurationHttpPacBrowserTest);
INSTANTIATE_TEST_CASES_FOR_TEST_FIXTURE(
    NetworkContextConfigurationFilePacBrowserTest);
INSTANTIATE_TEST_CASES_FOR_TEST_FIXTURE(
    NetworkContextConfigurationDataPacBrowserTest);
INSTANTIATE_TEST_CASES_FOR_TEST_FIXTURE(
    NetworkContextConfigurationFtpPacBrowserTest);

}  // namespace
