// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/webstore/webstore_provider.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_thread.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

using content::BrowserThread;
using net::test_server::BasicHttpResponse;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;
using net::test_server::EmbeddedTestServer;

namespace app_list {
namespace test {
namespace {

// Mock results.
const char kOneResult[] = "{"
    "\"search_url\": \"http://host/search\","
    "\"results\":["
      "{"
        "\"id\": \"app1_id\","
        "\"localized_name\": \"app1 name\","
        "\"icon_url\": \"http://host/icon\""
      "}"
    "]}";

const char kThreeResults[] = "{"
    "\"search_url\": \"http://host/search\","
    "\"results\":["
      "{"
        "\"id\": \"app1_id\","
        "\"localized_name\": \"one\","
        "\"icon_url\": \"http://host/icon\""
      "},"
      "{"
        "\"id\": \"app2_id\","
        "\"localized_name\": \"two\","
        "\"icon_url\": \"http://host/icon\""
      "},"
      "{"
        "\"id\": \"app3_id\","
        "\"localized_name\": \"three\","
        "\"icon_url\": \"http://host/icon\""
      "}"
    "]}";

}  // namespace

class WebstoreProviderTest : public InProcessBrowserTest {
 public:
  WebstoreProviderTest() {}
  virtual ~WebstoreProviderTest() {}

  // InProcessBrowserTest overrides:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitchASCII(switches::kForceFieldTrials,
                                    "LauncherUseWebstoreSearch/Enable/");
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    test_server_.reset(new EmbeddedTestServer);

    ASSERT_TRUE(test_server_->InitializeAndWaitUntilReady());
    test_server_->RegisterRequestHandler(
        base::Bind(&WebstoreProviderTest::HandleRequest,
                   base::Unretained(this)));
    CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kAppsGalleryURL, test_server_->base_url().spec());

    webstore_provider_.reset(new WebstoreProvider(
        ProfileManager::GetActiveUserProfile(), NULL));
    webstore_provider_->set_webstore_search_fetched_callback(
        base::Bind(&WebstoreProviderTest::OnSearchResultsFetched,
                   base::Unretained(this)));
    // TODO(mukai): add test cases for throttling.
    webstore_provider_->set_use_throttling(false);
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    EXPECT_TRUE(test_server_->ShutdownAndWaitUntilComplete());
    test_server_.reset();
  }

  std::string RunQuery(const std::string& query,
                       const std::string& mock_server_response) {
    webstore_provider_->Start(base::UTF8ToUTF16(query));

    if (webstore_provider_->webstore_search_ && !mock_server_response.empty()) {
      mock_server_response_ = mock_server_response;

      DCHECK(!run_loop_);
      run_loop_.reset(new base::RunLoop);
      run_loop_->Run();
      run_loop_.reset();

      mock_server_response_.clear();
    }

    webstore_provider_->Stop();
    return GetResults();
  }

  std::string GetResults() const {
    std::string results;
    for (SearchProvider::Results::const_iterator it =
             webstore_provider_->results().begin();
         it != webstore_provider_->results().end();
         ++it) {
      if (!results.empty())
        results += ',';
      results += base::UTF16ToUTF8((*it)->title());
    }
    return results;
  }

  WebstoreProvider* webstore_provider() { return webstore_provider_.get(); }

 private:
  scoped_ptr<HttpResponse> HandleRequest(const HttpRequest& request) {
    scoped_ptr<BasicHttpResponse> response(new BasicHttpResponse);

    if (request.relative_url.find("/jsonsearch?") != std::string::npos) {
      if (mock_server_response_ == "404") {
        response->set_code(net::HTTP_NOT_FOUND);
      } else if (mock_server_response_ == "500") {
        response->set_code(net::HTTP_INTERNAL_SERVER_ERROR);
      } else {
        response->set_code(net::HTTP_OK);
        response->set_content(mock_server_response_);
      }
    }

    return response.PassAs<HttpResponse>();
  }

  void OnSearchResultsFetched() {
    if (run_loop_)
      run_loop_->Quit();
  }

  scoped_ptr<EmbeddedTestServer> test_server_;
  scoped_ptr<base::RunLoop> run_loop_;

  std::string mock_server_response_;

  scoped_ptr<WebstoreProvider> webstore_provider_;

  DISALLOW_COPY_AND_ASSIGN(WebstoreProviderTest);
};

// Flaky on CrOS and Windows: http://crbug.com/246136.
// TODO(erg): linux_aura bringup: http://crbug.com/163931
#if defined(OS_WIN) || defined(OS_LINUX)
#define MAYBE_Basic DISABLED_Basic
#else
#define MAYBE_Basic Basic
#endif
IN_PROC_BROWSER_TEST_F(WebstoreProviderTest, MAYBE_Basic) {
  struct {
    const char* query;
    const char* mock_server_response;
    const char* expected_results_content;
  } kTestCases[] = {
    // "Search in web store" result with query text itself is used for
    // synchronous placeholder, bad server response etc.
    {"synchronous", "", "synchronous" },
    {"404", "404", "404" },
    {"500", "500", "500" },
    {"bad json", "invalid json", "bad json" },
    // Good results.
    {"1 result", kOneResult, "app1 name" },
    {"3 result", kThreeResults, "one,two,three" },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestCases); ++i) {
    EXPECT_EQ(kTestCases[i].expected_results_content,
              RunQuery(kTestCases[i].query,
                       kTestCases[i].mock_server_response))
        << "Case " << i << ": q=" << kTestCases[i].query;
  }
}

IN_PROC_BROWSER_TEST_F(WebstoreProviderTest, NoSearchForSensitiveData) {
  // None of the following input strings should be accepted because they may
  // contain private data.
  const char* inputs[] = {
    // file: scheme is bad.
    "file://filename",
    "FILE://filename",
    // URLs with usernames, ports, queries or refs are bad.
    "http://username:password@hostname/",
    "http://www.example.com:1000",
    "http://foo:1000",
    "http://hostname/?query=q",
    "http://hostname/path#ref",
    // A https URL with path is bad.
    "https://hostname/path",
  };

  for (size_t i = 0; i < arraysize(inputs); ++i)
    EXPECT_EQ("", RunQuery(inputs[i], kOneResult));
}

IN_PROC_BROWSER_TEST_F(WebstoreProviderTest, NoSearchForShortQueries) {
  EXPECT_EQ("", RunQuery("a", kOneResult));
  EXPECT_EQ("", RunQuery("ab", kOneResult));
  EXPECT_EQ("app1 name", RunQuery("abc", kOneResult));
}

// Flaky on CrOS and Windows: http://crbug.com/246136.
#if defined(OS_WIN) || defined(OS_CHROMEOS)
#define MAYBE_SearchCache DISABLED_SearchCache
#else
#define MAYBE_SearchCache SearchCache
#endif
IN_PROC_BROWSER_TEST_F(WebstoreProviderTest, MAYBE_SearchCache) {
  EXPECT_EQ("app1 name", RunQuery("foo", kOneResult));

  // No result is provided but the provider gets the result from the cache.
  EXPECT_EQ("app1 name", RunQuery("foo", ""));
}

}  // namespace test
}  // namespace app_list
