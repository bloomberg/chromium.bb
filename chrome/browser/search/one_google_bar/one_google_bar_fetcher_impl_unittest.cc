// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/one_google_bar/one_google_bar_fetcher_impl.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/mock_callback.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/search/one_google_bar/one_google_bar_data.h"
#include "components/google/core/browser/google_url_tracker.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_service_manager_context.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "services/data_decoder/public/cpp/testing_json_parser.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Eq;
using testing::IsEmpty;
using testing::SaveArg;
using testing::StartsWith;

namespace {

const char kApplicationLocale[] = "de";

const char kMinimalValidResponse[] = R"json({"update": { "ogb": {
  "html": { "private_do_not_access_or_else_safe_html_wrapped_value": "" },
  "page_hooks": {}
}}})json";

// Required to instantiate a GoogleUrlTracker in UNIT_TEST_MODE.
class GoogleURLTrackerClientStub : public GoogleURLTrackerClient {
 public:
  GoogleURLTrackerClientStub() {}
  ~GoogleURLTrackerClientStub() override {}

  bool IsBackgroundNetworkingEnabled() override { return true; }
  PrefService* GetPrefs() override { return nullptr; }
  net::URLRequestContextGetter* GetRequestContext() override { return nullptr; }

 private:
  DISALLOW_COPY_AND_ASSIGN(GoogleURLTrackerClientStub);
};

}  // namespace

class OneGoogleBarFetcherImplTest : public testing::Test {
 public:
  OneGoogleBarFetcherImplTest() : OneGoogleBarFetcherImplTest(base::nullopt) {}

  OneGoogleBarFetcherImplTest(
      const base::Optional<std::string>& api_url_override)
      : task_runner_(new base::TestSimpleTaskRunner()),
        request_context_getter_(
            new net::TestURLRequestContextGetter(task_runner_)),
        google_url_tracker_(base::MakeUnique<GoogleURLTrackerClientStub>(),
                            GoogleURLTracker::UNIT_TEST_MODE),
        one_google_bar_fetcher_(request_context_getter_.get(),
                                &google_url_tracker_,
                                kApplicationLocale,
                                api_url_override) {}

  net::TestURLFetcher* GetRunningURLFetcher() {
    // All created URLFetchers have ID 0 by default.
    net::TestURLFetcher* url_fetcher = url_fetcher_factory_.GetFetcherByID(0);
    DCHECK(url_fetcher);
    return url_fetcher;
  }

  void RespondWithData(const std::string& data) {
    net::TestURLFetcher* url_fetcher = GetRunningURLFetcher();
    url_fetcher->set_status(net::URLRequestStatus());
    url_fetcher->set_response_code(net::HTTP_OK);
    url_fetcher->SetResponseString(data);
    url_fetcher->delegate()->OnURLFetchComplete(url_fetcher);
    // SafeJsonParser is asynchronous.
    base::RunLoop().RunUntilIdle();
  }

  void RespondWithNetworkError() {
    net::TestURLFetcher* url_fetcher = GetRunningURLFetcher();
    url_fetcher->set_status(net::URLRequestStatus::FromError(net::ERR_FAILED));
    url_fetcher->delegate()->OnURLFetchComplete(url_fetcher);
  }

  void RespondWithHttpError() {
    net::TestURLFetcher* url_fetcher = GetRunningURLFetcher();
    url_fetcher->set_status(net::URLRequestStatus());
    url_fetcher->set_response_code(net::HTTP_NOT_FOUND);
    url_fetcher->delegate()->OnURLFetchComplete(url_fetcher);
  }

  OneGoogleBarFetcherImpl* one_google_bar_fetcher() {
    return &one_google_bar_fetcher_;
  }

 private:
  // variations::AppendVariationHeaders and SafeJsonParser require a
  // threads and a ServiceManagerConnection to be set.
  content::TestBrowserThreadBundle thread_bundle_;
  content::TestServiceManagerContext service_manager_context_;

  data_decoder::TestingJsonParser::ScopedFactoryOverride factory_override_;

  net::TestURLFetcherFactory url_fetcher_factory_;

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_getter_;
  GoogleURLTracker google_url_tracker_;

  OneGoogleBarFetcherImpl one_google_bar_fetcher_;
};

TEST_F(OneGoogleBarFetcherImplTest, RequestUrlContainsLanguage) {
  // Trigger a request.
  base::MockCallback<OneGoogleBarFetcher::OneGoogleCallback> callback;
  one_google_bar_fetcher()->Fetch(callback.Get());

  net::TestURLFetcher* fetcher = GetRunningURLFetcher();
  GURL request_url = fetcher->GetOriginalURL();

  // Make sure the request URL contains the "hl=" query param.
  std::string expected_query = base::StringPrintf("hl=%s", kApplicationLocale);
  EXPECT_EQ(expected_query, request_url.query());
}

TEST_F(OneGoogleBarFetcherImplTest, RequestReturns) {
  base::MockCallback<OneGoogleBarFetcher::OneGoogleCallback> callback;
  one_google_bar_fetcher()->Fetch(callback.Get());

  base::Optional<OneGoogleBarData> data;
  EXPECT_CALL(callback, Run(OneGoogleBarFetcher::Status::OK, _))
      .WillOnce(SaveArg<1>(&data));
  RespondWithData(kMinimalValidResponse);

  EXPECT_TRUE(data.has_value());
}

TEST_F(OneGoogleBarFetcherImplTest, HandlesResponsePreamble) {
  base::MockCallback<OneGoogleBarFetcher::OneGoogleCallback> callback;
  one_google_bar_fetcher()->Fetch(callback.Get());

  // The reponse may contain a ")]}'" prefix. The fetcher should ignore that
  // during parsing.
  base::Optional<OneGoogleBarData> data;
  EXPECT_CALL(callback, Run(OneGoogleBarFetcher::Status::OK, _))
      .WillOnce(SaveArg<1>(&data));
  RespondWithData(std::string(")]}'") + kMinimalValidResponse);

  EXPECT_TRUE(data.has_value());
}

TEST_F(OneGoogleBarFetcherImplTest, ParsesFullResponse) {
  base::MockCallback<OneGoogleBarFetcher::OneGoogleCallback> callback;
  one_google_bar_fetcher()->Fetch(callback.Get());

  base::Optional<OneGoogleBarData> data;
  EXPECT_CALL(callback, Run(OneGoogleBarFetcher::Status::OK, _))
      .WillOnce(SaveArg<1>(&data));
  RespondWithData(R"json({"update": { "ogb": {
    "html": {
      "private_do_not_access_or_else_safe_html_wrapped_value": "bar_html_value"
    },
    "page_hooks": {
      "in_head_script": {
        "private_do_not_access_or_else_safe_script_wrapped_value":
          "in_head_script_value"
      },
      "in_head_style": {
        "private_do_not_access_or_else_safe_style_sheet_wrapped_value":
          "in_head_style_value"
      },
      "after_bar_script": {
        "private_do_not_access_or_else_safe_script_wrapped_value":
          "after_bar_script_value"
      },
      "end_of_body_html": {
        "private_do_not_access_or_else_safe_html_wrapped_value":
          "end_of_body_html_value"
      },
      "end_of_body_script": {
        "private_do_not_access_or_else_safe_script_wrapped_value":
          "end_of_body_script_value"
      }
    }
  }}})json");

  ASSERT_TRUE(data.has_value());
  EXPECT_THAT(data->bar_html, Eq("bar_html_value"));
  EXPECT_THAT(data->in_head_script, Eq("in_head_script_value"));
  EXPECT_THAT(data->in_head_style, Eq("in_head_style_value"));
  EXPECT_THAT(data->after_bar_script, Eq("after_bar_script_value"));
  EXPECT_THAT(data->end_of_body_html, Eq("end_of_body_html_value"));
  EXPECT_THAT(data->end_of_body_script, Eq("end_of_body_script_value"));
}

class MockURLFetcherDelegateForTests
    : public net::TestURLFetcher::DelegateForTests {
 public:
  MOCK_METHOD1(OnRequestStart, void(int fetcher_id));
  MOCK_METHOD1(OnChunkUpload, void(int fetcher_id));
  MOCK_METHOD1(OnRequestEnd, void(int fetcher_id));
};

TEST_F(OneGoogleBarFetcherImplTest, SecondRequestOverridesFirst) {
  // Trigger the first request.
  base::MockCallback<OneGoogleBarFetcher::OneGoogleCallback> first_callback;
  one_google_bar_fetcher()->Fetch(first_callback.Get());
  net::TestURLFetcher* first_fetcher = GetRunningURLFetcher();
  testing::StrictMock<MockURLFetcherDelegateForTests> first_mock_delegate;
  first_fetcher->SetDelegateForTests(&first_mock_delegate);

  // Trigger a second request. That should cancel the first one.
  EXPECT_CALL(first_mock_delegate, OnRequestEnd(0));
  base::MockCallback<OneGoogleBarFetcher::OneGoogleCallback> second_callback;
  one_google_bar_fetcher()->Fetch(second_callback.Get());
}

TEST_F(OneGoogleBarFetcherImplTest, CoalescesMultipleRequests) {
  // Trigger two requests.
  base::MockCallback<OneGoogleBarFetcher::OneGoogleCallback> first_callback;
  one_google_bar_fetcher()->Fetch(first_callback.Get());
  base::MockCallback<OneGoogleBarFetcher::OneGoogleCallback> second_callback;
  one_google_bar_fetcher()->Fetch(second_callback.Get());

  // Make sure that a single response causes both callbacks to be called.
  base::Optional<OneGoogleBarData> first_data;
  base::Optional<OneGoogleBarData> second_data;

  EXPECT_CALL(first_callback, Run(OneGoogleBarFetcher::Status::OK, _))
      .WillOnce(SaveArg<1>(&first_data));
  EXPECT_CALL(second_callback, Run(OneGoogleBarFetcher::Status::OK, _))
      .WillOnce(SaveArg<1>(&second_data));

  RespondWithData(kMinimalValidResponse);

  // Ensure that both requests received a response.
  EXPECT_TRUE(first_data.has_value());
  EXPECT_TRUE(second_data.has_value());
}

TEST_F(OneGoogleBarFetcherImplTest, NetworkErrorIsTransient) {
  base::MockCallback<OneGoogleBarFetcher::OneGoogleCallback> callback;
  one_google_bar_fetcher()->Fetch(callback.Get());

  EXPECT_CALL(callback, Run(OneGoogleBarFetcher::Status::TRANSIENT_ERROR,
                            Eq(base::nullopt)));
  RespondWithNetworkError();
}

TEST_F(OneGoogleBarFetcherImplTest, HttpErrorIsFatal) {
  base::MockCallback<OneGoogleBarFetcher::OneGoogleCallback> callback;
  one_google_bar_fetcher()->Fetch(callback.Get());

  EXPECT_CALL(callback,
              Run(OneGoogleBarFetcher::Status::FATAL_ERROR, Eq(base::nullopt)));
  RespondWithHttpError();
}

TEST_F(OneGoogleBarFetcherImplTest, InvalidJsonErrorIsFatal) {
  base::MockCallback<OneGoogleBarFetcher::OneGoogleCallback> callback;
  one_google_bar_fetcher()->Fetch(callback.Get());

  EXPECT_CALL(callback,
              Run(OneGoogleBarFetcher::Status::FATAL_ERROR, Eq(base::nullopt)));
  RespondWithData(kMinimalValidResponse + std::string(")"));
}

TEST_F(OneGoogleBarFetcherImplTest, IncompleteJsonErrorIsFatal) {
  base::MockCallback<OneGoogleBarFetcher::OneGoogleCallback> callback;
  one_google_bar_fetcher()->Fetch(callback.Get());

  EXPECT_CALL(callback,
              Run(OneGoogleBarFetcher::Status::FATAL_ERROR, Eq(base::nullopt)));
  RespondWithData(R"json({"update": { "ogb": {
  "html": {},
  "page_hooks": {}
}}})json");
}

class OneGoogleBarFetcherImplWithRelativeApiUrlOverrideTest
    : public OneGoogleBarFetcherImplTest {
 public:
  OneGoogleBarFetcherImplWithRelativeApiUrlOverrideTest()
      : OneGoogleBarFetcherImplTest(std::string("/testapi?q=a")) {}
};

TEST_F(OneGoogleBarFetcherImplWithRelativeApiUrlOverrideTest,
       RequestUrlRespectsOverride) {
  // Trigger a request.
  base::MockCallback<OneGoogleBarFetcher::OneGoogleCallback> callback;
  one_google_bar_fetcher()->Fetch(callback.Get());

  // Make sure the request URL corresponds to the override, but also contains
  // the "hl=" query param.
  GURL request_url = GetRunningURLFetcher()->GetOriginalURL();
  EXPECT_EQ("/testapi", request_url.path());
  std::string expected_query =
      base::StringPrintf("q=a&hl=%s", kApplicationLocale);
  EXPECT_EQ(expected_query, request_url.query());
}

class OneGoogleBarFetcherImplWithAbsoluteApiUrlOverrideTest
    : public OneGoogleBarFetcherImplTest {
 public:
  OneGoogleBarFetcherImplWithAbsoluteApiUrlOverrideTest()
      : OneGoogleBarFetcherImplTest(std::string("http://test.com/path?q=a")) {}
};

TEST_F(OneGoogleBarFetcherImplWithAbsoluteApiUrlOverrideTest,
       RequestUrlRespectsOverride) {
  // Trigger a request.
  base::MockCallback<OneGoogleBarFetcher::OneGoogleCallback> callback;
  one_google_bar_fetcher()->Fetch(callback.Get());

  // Make sure the request URL corresponds to the override, but also contains
  // the "hl=" query param.
  GURL request_url = GetRunningURLFetcher()->GetOriginalURL();
  GURL expected_url = GURL(
      base::StringPrintf("http://test.com/path?q=a&hl=%s", kApplicationLocale));
  EXPECT_EQ(expected_url, request_url);
}
