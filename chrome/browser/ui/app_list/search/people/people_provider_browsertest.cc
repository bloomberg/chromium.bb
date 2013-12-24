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
#include "chrome/browser/ui/app_list/search/people/people_provider.h"
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
    "\"items\":["
      "{"
        "\"person\" : {"
          "\"id\": \"1\","
          "\"metadata\" : {"
            "\"ownerId\": \"1\""
          "},"
          "\"names\" : [{"
            "\"displayName\": \"first person\""
          "}],"
          "\"emails\" : [{"
            "\"value\": \"first@person.com\""
          "}],"
          "\"images\" : [{"
            "\"url\": \"http://host/icon\""
          "}],"
          "\"sortKeys\" : {"
            "\"interactionRank\": \"0.98\""
          "}"
        "}"
      "}"
    "]}";

const char kThreeValidResults[] = "{"
    "\"items\":["
      "{"
        "\"person\" : {"
          "\"id\": \"1\","
          "\"metadata\" : {"
            "\"ownerId\": \"1\""
          "},"
          "\"names\" : [{"
            "\"displayName\": \"first person\""
          "}],"
          "\"emails\" : [{"
            "\"value\": \"first@person.com\""
          "}],"
          "\"images\" : [{"
            "\"url\": \"http://host/icon\""
          "}],"
          "\"sortKeys\" : {"
            "\"interactionRank\": \"0.98\""
          "}"
        "}"
      "},"
      "{"
        "\"person\" : {"
          "\"id\": \"2\","
          "\"metadata\" : {"
            "\"ownerId\": \"37\""
          "},"
          "\"names\" : [{"
            "\"displayName\": \"second person\""
          "}],"
          "\"emails\" : [{"
            "\"value\": \"second@person.com\""
          "}],"
          "\"images\" : [{"
            "\"url\": \"http://host/icon\""
          "}],"
          "\"sortKeys\" : {"
            "\"interactionRank\": \"0.84\""
          "}"
        "}"
      "},"
      "{"
        "\"person\" : {"
          "\"id\": \"3\","
          "\"metadata\" : {"
            "\"ownerId\": \"3\""
          "},"
          "\"names\" : [{"
            "\"displayName\": \"third person\""
          "}],"
          "\"emails\" : [{"
            "\"value\": \"third@person.com\""
          "}],"
          "\"images\" : [{"
            "\"url\": \"http://host/icon\""
          "}],"
          "\"sortKeys\" : {"
            "\"interactionRank\": \"0.67\""
          "}"
        "}"
      "},"
      "{"
        "\"person\" : {"
          "\"id\": \"4\","
          "\"metadata\" : {"
            "\"ownerId\": \"4\""
          "},"
          "\"names\" : [{"
            "\"displayName\": \"fourth person\""
          "}],"
          "\"emails\" : [{"
            "\"value\": \"fourth@person.com\""
          "}],"
          "\"images\" : [{"
            "\"url\": \"http://host/icon\""
          "}],"
          "\"sortKeys\" : {"
            "\"interactionRank\": \"0.0\""
          "}"
        "}"
      "},"
      "{"
        "\"person\" : {"
          "\"id\": \"5\","
          "\"metadata\" : {"
            "\"ownerId\": \"5\""
          "},"
          "\"names\" : [{"
            "\"displayName\": \"fifth person\""
          "}],"
          "\"emails\" : [{"
            "\"value\": \"fifth@person.com\""
          "}],"
          // Images field is missing on purpose.
          "\"sortKeys\" : {"
            "\"interactionRank\": \"0.98\""
          "}"
        "}"
      "}"
    "]}";

}  // namespace

class PeopleProviderTest : public InProcessBrowserTest {
 public:
  PeopleProviderTest() {}
  virtual ~PeopleProviderTest() {}

  // InProcessBrowserTest overrides:
  virtual void SetUpOnMainThread() OVERRIDE {
    test_server_.reset(new EmbeddedTestServer);

    ASSERT_TRUE(test_server_->InitializeAndWaitUntilReady());
    test_server_->RegisterRequestHandler(
        base::Bind(&PeopleProviderTest::HandleRequest,
                   base::Unretained(this)));

    people_provider_.reset(new PeopleProvider(
        ProfileManager::GetActiveUserProfile()));

    people_provider_->SetupForTest(
        base::Bind(&PeopleProviderTest::OnSearchResultsFetched,
                   base::Unretained(this)),
        test_server_->base_url());
    people_provider_->set_use_throttling(false);
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    EXPECT_TRUE(test_server_->ShutdownAndWaitUntilComplete());
    test_server_.reset();
  }

  std::string RunQuery(const std::string& query,
                       const std::string& mock_server_response) {
    people_provider_->Start(base::UTF8ToUTF16(query));

    if (people_provider_->people_search_ && !mock_server_response.empty()) {
      mock_server_response_ = mock_server_response;

      DCHECK(!run_loop_);
      run_loop_.reset(new base::RunLoop);
      run_loop_->Run();
      run_loop_.reset();

      mock_server_response_.clear();
    }

    people_provider_->Stop();
    return GetResults();
  }

  std::string GetResults() const {
    std::string results;
    for (SearchProvider::Results::const_iterator it =
             people_provider_->results().begin();
         it != people_provider_->results().end();
         ++it) {
      if (!results.empty())
        results += ',';
      results += base::UTF16ToUTF8((*it)->title());
    }
    return results;
  }

  PeopleProvider* people_provider() { return people_provider_.get(); }

 private:
  scoped_ptr<HttpResponse> HandleRequest(const HttpRequest& request) {
    scoped_ptr<BasicHttpResponse> response(new BasicHttpResponse);
    response->set_code(net::HTTP_OK);
    response->set_content(mock_server_response_);

    return response.PassAs<HttpResponse>();
  }

  void OnSearchResultsFetched() {
    if (run_loop_)
      run_loop_->Quit();
  }

  scoped_ptr<EmbeddedTestServer> test_server_;
  scoped_ptr<base::RunLoop> run_loop_;

  std::string mock_server_response_;

  scoped_ptr<PeopleProvider> people_provider_;

  DISALLOW_COPY_AND_ASSIGN(PeopleProviderTest);
};

IN_PROC_BROWSER_TEST_F(PeopleProviderTest, Basic) {
  struct {
    const char* query;
    const char* mock_server_response;
    const char* expected_results_content;
  } kTestCases[] = {
    {"first", kOneResult, "first person" },
    {"person", kThreeValidResults, "first person,second person,third person" },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestCases); ++i) {
    EXPECT_EQ(kTestCases[i].expected_results_content,
              RunQuery(kTestCases[i].query,
                       kTestCases[i].mock_server_response))
        << "Case " << i << ": q=" << kTestCases[i].query;
  }
}

IN_PROC_BROWSER_TEST_F(PeopleProviderTest, NoSearchForSensitiveData) {
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

IN_PROC_BROWSER_TEST_F(PeopleProviderTest, NoSearchForShortQueries) {
  EXPECT_EQ("", RunQuery("f", kOneResult));
  EXPECT_EQ("", RunQuery("fi", kOneResult));
  EXPECT_EQ("first person", RunQuery("fir", kOneResult));
}

}  // namespace test
}  // namespace app_list
