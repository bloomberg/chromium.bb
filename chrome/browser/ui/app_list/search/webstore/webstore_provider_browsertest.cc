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
#include "chrome/browser/ui/app_list/search/webstore/webstore_result.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/extension.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

using content::BrowserThread;
using extensions::Manifest;
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
        "\"icon_url\": \"http://host/icon\","
        "\"is_paid\": false"
      "}"
    "]}";

const char kThreeResults[] = "{"
    "\"search_url\": \"http://host/search\","
    "\"results\":["
      "{"
        "\"id\": \"app1_id\","
        "\"localized_name\": \"one\","
        "\"icon_url\": \"http://host/icon1\","
        "\"is_paid\": true,"
        "\"item_type\": \"PLATFORM_APP\""
      "},"
      "{"
        "\"id\": \"app2_id\","
        "\"localized_name\": \"two\","
        "\"icon_url\": \"http://host/icon2\","
        "\"is_paid\": false,"
        "\"item_type\": \"HOSTED_APP\""
      "},"
      "{"
        "\"id\": \"app3_id\","
        "\"localized_name\": \"three\","
        "\"icon_url\": \"http://host/icon3\","
        "\"is_paid\": false,"
        "\"item_type\": \"LEGACY_PACKAGED_APP\""
      "}"
    "]}";

struct ParsedSearchResult {
  const char* id;
  const char* title;
  const char* icon_url;
  bool is_paid;
  Manifest::Type item_type;
  size_t num_actions;
};

ParsedSearchResult kParsedOneResult[] = {{"app1_id", "app1 name",
                                          "http://host/icon", false,
                                          Manifest::TYPE_UNKNOWN, 1}};

ParsedSearchResult kParsedThreeResults[] = {
    {"app1_id", "one", "http://host/icon1", true, Manifest::TYPE_PLATFORM_APP,
     1},
    {"app2_id", "two", "http://host/icon2", false, Manifest::TYPE_HOSTED_APP,
     2},
    {"app3_id", "three", "http://host/icon3", false,
     Manifest::TYPE_LEGACY_PACKAGED_APP, 1}};

}  // namespace

class WebstoreProviderTest : public InProcessBrowserTest {
 public:
  WebstoreProviderTest() {}
  virtual ~WebstoreProviderTest() {}

  // InProcessBrowserTest overrides:
  virtual void SetUpOnMainThread() OVERRIDE {
    test_server_.reset(new EmbeddedTestServer);

    ASSERT_TRUE(test_server_->InitializeAndWaitUntilReady());
    test_server_->RegisterRequestHandler(
        base::Bind(&WebstoreProviderTest::HandleRequest,
                   base::Unretained(this)));
    CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kAppsGalleryURL, test_server_->base_url().spec());
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableEphemeralApps);

    webstore_provider_.reset(new WebstoreProvider(
        ProfileManager::GetActiveUserProfile(), NULL));
    webstore_provider_->set_webstore_search_fetched_callback(
        base::Bind(&WebstoreProviderTest::OnSearchResultsFetched,
                   base::Unretained(this)));
    // TODO(mukai): add test cases for throttling.
    webstore_provider_->set_use_throttling(false);
  }

  virtual void TearDownOnMainThread() OVERRIDE {
    EXPECT_TRUE(test_server_->ShutdownAndWaitUntilComplete());
    test_server_.reset();
  }

  void RunQuery(const std::string& query,
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
  }

  std::string GetResultTitles() const {
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

  void VerifyResults(const ParsedSearchResult* expected_results,
                     size_t expected_result_size) {
    ASSERT_EQ(expected_result_size, webstore_provider_->results().size());
    for (size_t i = 0; i < expected_result_size; ++i) {
      const SearchResult* result = webstore_provider_->results()[i];
      ASSERT_EQ(extensions::Extension::GetBaseURLFromExtensionId(
                    expected_results[i].id).spec(),
                result->id());
      EXPECT_EQ(std::string(expected_results[i].title),
                base::UTF16ToUTF8(result->title()));

      // Ensure the number of action buttons is appropriate for the item type.
      EXPECT_EQ(expected_results[i].num_actions, result->actions().size());

      const WebstoreResult* webstore_result =
          static_cast<const WebstoreResult*>(result);
      EXPECT_EQ(expected_results[i].id, webstore_result->app_id());
      EXPECT_EQ(expected_results[i].icon_url,
                webstore_result->icon_url().spec());
      EXPECT_EQ(expected_results[i].is_paid, webstore_result->is_paid());
      EXPECT_EQ(expected_results[i].item_type, webstore_result->item_type());
    }
  }

  void RunQueryAndVerify(const std::string& query,
                         const std::string& mock_server_response,
                         const ParsedSearchResult* expected_results,
                         size_t expected_result_size) {
    RunQuery(query, mock_server_response);
    VerifyResults(expected_results, expected_result_size);
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
    const char* expected_result_titles;
    const ParsedSearchResult* expected_results;
    size_t expected_result_size;
  } kTestCases[] = {
    // "Search in web store" result with query text itself is used for
    // synchronous placeholder, bad server response etc.
    {"synchronous", "", "synchronous", NULL, 0 },
    {"404", "404", "404", NULL, 0 },
    {"500", "500", "500", NULL, 0 },
    {"bad json", "invalid json", "bad json", NULL, 0 },
    // Good results.
    {"1 result", kOneResult, "app1 name", kParsedOneResult, 1 },
    {"3 result", kThreeResults, "one,two,three", kParsedThreeResults, 3 },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestCases); ++i) {
    if (kTestCases[i].expected_result_titles) {
      RunQuery(kTestCases[i].query, kTestCases[i].mock_server_response);
      ASSERT_EQ(kTestCases[i].expected_result_titles, GetResultTitles())
          << "Case " << i << ": q=" << kTestCases[i].query;

      if (kTestCases[i].expected_results) {
        VerifyResults(kTestCases[i].expected_results,
                      kTestCases[i].expected_result_size);
      }
    }
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

  for (size_t i = 0; i < arraysize(inputs); ++i) {
    RunQueryAndVerify(inputs[i], kOneResult, NULL, 0);
  }
}

IN_PROC_BROWSER_TEST_F(WebstoreProviderTest, NoSearchForShortQueries) {
  RunQueryAndVerify("a", kOneResult, NULL, 0);
  RunQueryAndVerify("ab", kOneResult, NULL, 0);
  RunQueryAndVerify("abc", kOneResult, kParsedOneResult, 1);
}

// Flaky on CrOS and Windows: http://crbug.com/246136.
#if defined(OS_WIN) || defined(OS_CHROMEOS)
#define MAYBE_SearchCache DISABLED_SearchCache
#else
#define MAYBE_SearchCache SearchCache
#endif
IN_PROC_BROWSER_TEST_F(WebstoreProviderTest, MAYBE_SearchCache) {
  RunQueryAndVerify("foo", kOneResult, kParsedOneResult, 1);

  // No result is provided but the provider gets the result from the cache.
  RunQueryAndVerify("foo", "", kParsedOneResult, 1);
}

}  // namespace test
}  // namespace app_list
