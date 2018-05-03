// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/one_google_bar/one_google_bar_fetcher_impl.h"

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/search/one_google_bar/one_google_bar_data.h"
#include "components/google/core/browser/google_url_tracker.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "content/public/common/weak_wrapper_shared_url_loader_factory.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_service_manager_context.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "services/data_decoder/public/cpp/testing_json_parser.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Eq;
using testing::IsEmpty;
using testing::SaveArg;
using testing::StartsWith;

// Needed for GoogleURLTrackerClientStub below.
namespace net {
class URLRequestContextGetter;
}

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

ACTION_P(Quit, run_loop) {
  run_loop->Quit();
}

class OneGoogleBarFetcherImplTest : public testing::Test {
 public:
  OneGoogleBarFetcherImplTest()
      : OneGoogleBarFetcherImplTest(
            /*api_url_override=*/base::nullopt,
            /*account_consistency_mirror_required=*/false) {}

  explicit OneGoogleBarFetcherImplTest(
      const base::Optional<std::string>& api_url_override)
      : OneGoogleBarFetcherImplTest(
            api_url_override,
            /*account_consistency_mirror_required=*/false) {}

  explicit OneGoogleBarFetcherImplTest(bool account_consistency_mirror_required)
      : OneGoogleBarFetcherImplTest(/*api_url_override=*/base::nullopt,
                                    account_consistency_mirror_required) {}

  OneGoogleBarFetcherImplTest(
      const base::Optional<std::string>& api_url_override,
      bool account_consistency_mirror_required)
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        google_url_tracker_(std::make_unique<GoogleURLTrackerClientStub>(),
                            GoogleURLTracker::ALWAYS_DOT_COM_MODE),
        test_shared_loader_factory_(
            base::MakeRefCounted<content::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)),
        api_url_override_(api_url_override),
        account_consistency_mirror_required_(
            account_consistency_mirror_required) {}

  ~OneGoogleBarFetcherImplTest() override {
    static_cast<KeyedService&>(google_url_tracker_).Shutdown();
  }

  void SetUp() override {
    testing::Test::SetUp();

    one_google_bar_fetcher_ = std::make_unique<OneGoogleBarFetcherImpl>(
        test_shared_loader_factory_, &google_url_tracker_, kApplicationLocale,
        api_url_override_, account_consistency_mirror_required_);
  }

  void SetUpResponseWithData(const std::string& response) {
    test_url_loader_factory_.SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& request) {
          last_request_url_ = request.url;
          last_request_headers_ = request.headers;
        }));
    test_url_loader_factory_.AddResponse(
        one_google_bar_fetcher_->GetFetchURLForTesting().spec(), response);
  }

  void SetUpResponseWithNetworkError() {
    test_url_loader_factory_.AddResponse(
        one_google_bar_fetcher_->GetFetchURLForTesting(),
        network::ResourceResponseHead(), std::string(),
        network::URLLoaderCompletionStatus(net::HTTP_NOT_FOUND));
  }

  OneGoogleBarFetcherImpl* one_google_bar_fetcher() {
    return one_google_bar_fetcher_.get();
  }

  GURL last_request_url() { return last_request_url_; }
  net::HttpRequestHeaders last_request_headers() {
    return last_request_headers_;
  }

 private:
  // variations::AppendVariationHeaders and SafeJsonParser require a
  // threads and a ServiceManagerConnection to be set.
  content::TestBrowserThreadBundle thread_bundle_;
  content::TestServiceManagerContext service_manager_context_;

  data_decoder::TestingJsonParser::ScopedFactoryOverride factory_override_;

  GoogleURLTracker google_url_tracker_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  base::Optional<std::string> api_url_override_;
  bool account_consistency_mirror_required_;

  GURL last_request_url_;
  net::HttpRequestHeaders last_request_headers_;

  std::unique_ptr<OneGoogleBarFetcherImpl> one_google_bar_fetcher_;
};

TEST_F(OneGoogleBarFetcherImplTest, RequestUrlContainsLanguage) {
  SetUpResponseWithData(kMinimalValidResponse);

  // Trigger a request.
  base::MockCallback<OneGoogleBarFetcher::OneGoogleCallback> callback;
  one_google_bar_fetcher()->Fetch(callback.Get());

  base::RunLoop loop;
  EXPECT_CALL(callback, Run(_, _)).WillOnce(Quit(&loop));
  loop.Run();

  // Make sure the request URL contains the "hl=" query param.
  std::string expected_query =
      base::StringPrintf("hl=%s&async=fixed:0", kApplicationLocale);
  EXPECT_EQ(expected_query, last_request_url().query());
}

TEST_F(OneGoogleBarFetcherImplTest, RequestReturns) {
  SetUpResponseWithData(kMinimalValidResponse);

  base::MockCallback<OneGoogleBarFetcher::OneGoogleCallback> callback;
  one_google_bar_fetcher()->Fetch(callback.Get());

  base::Optional<OneGoogleBarData> data;
  base::RunLoop loop;
  EXPECT_CALL(callback, Run(OneGoogleBarFetcher::Status::OK, _))
      .WillOnce(DoAll(SaveArg<1>(&data), Quit(&loop)));
  loop.Run();

  EXPECT_TRUE(data.has_value());
}

TEST_F(OneGoogleBarFetcherImplTest, HandlesResponsePreamble) {
  // The reponse may contain a ")]}'" prefix. The fetcher should ignore that
  // during parsing.
  SetUpResponseWithData(std::string(")]}'") + kMinimalValidResponse);

  base::MockCallback<OneGoogleBarFetcher::OneGoogleCallback> callback;
  one_google_bar_fetcher()->Fetch(callback.Get());

  base::Optional<OneGoogleBarData> data;
  base::RunLoop loop;
  EXPECT_CALL(callback, Run(OneGoogleBarFetcher::Status::OK, _))
      .WillOnce(DoAll(SaveArg<1>(&data), Quit(&loop)));
  loop.Run();

  EXPECT_TRUE(data.has_value());
}

TEST_F(OneGoogleBarFetcherImplTest, ParsesFullResponse) {
  SetUpResponseWithData(R"json({"update": { "ogb": {
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

  base::MockCallback<OneGoogleBarFetcher::OneGoogleCallback> callback;
  one_google_bar_fetcher()->Fetch(callback.Get());

  base::Optional<OneGoogleBarData> data;
  base::RunLoop loop;
  EXPECT_CALL(callback, Run(OneGoogleBarFetcher::Status::OK, _))
      .WillOnce(DoAll(SaveArg<1>(&data), Quit(&loop)));
  loop.Run();

  ASSERT_TRUE(data.has_value());
  EXPECT_THAT(data->bar_html, Eq("bar_html_value"));
  EXPECT_THAT(data->in_head_script, Eq("in_head_script_value"));
  EXPECT_THAT(data->in_head_style, Eq("in_head_style_value"));
  EXPECT_THAT(data->after_bar_script, Eq("after_bar_script_value"));
  EXPECT_THAT(data->end_of_body_html, Eq("end_of_body_html_value"));
  EXPECT_THAT(data->end_of_body_script, Eq("end_of_body_script_value"));
}

TEST_F(OneGoogleBarFetcherImplTest, CoalescesMultipleRequests) {
  SetUpResponseWithData(kMinimalValidResponse);

  // Trigger two requests.
  base::MockCallback<OneGoogleBarFetcher::OneGoogleCallback> first_callback;
  one_google_bar_fetcher()->Fetch(first_callback.Get());
  base::MockCallback<OneGoogleBarFetcher::OneGoogleCallback> second_callback;
  one_google_bar_fetcher()->Fetch(second_callback.Get());

  // Make sure that a single response causes both callbacks to be called.
  base::Optional<OneGoogleBarData> first_data;
  base::Optional<OneGoogleBarData> second_data;

  base::RunLoop loop;
  EXPECT_CALL(first_callback, Run(OneGoogleBarFetcher::Status::OK, _))
      .WillOnce(SaveArg<1>(&first_data));
  EXPECT_CALL(second_callback, Run(OneGoogleBarFetcher::Status::OK, _))
      .WillOnce(DoAll(SaveArg<1>(&second_data), Quit(&loop)));
  loop.Run();

  // Ensure that both requests received a response.
  EXPECT_TRUE(first_data.has_value());
  EXPECT_TRUE(second_data.has_value());
}

TEST_F(OneGoogleBarFetcherImplTest, NetworkErrorIsTransient) {
  SetUpResponseWithNetworkError();

  base::MockCallback<OneGoogleBarFetcher::OneGoogleCallback> callback;
  one_google_bar_fetcher()->Fetch(callback.Get());

  base::RunLoop loop;
  EXPECT_CALL(callback, Run(OneGoogleBarFetcher::Status::TRANSIENT_ERROR,
                            Eq(base::nullopt)))
      .WillOnce(Quit(&loop));
  loop.Run();
}

TEST_F(OneGoogleBarFetcherImplTest, InvalidJsonErrorIsFatal) {
  SetUpResponseWithData(kMinimalValidResponse + std::string(")"));

  base::MockCallback<OneGoogleBarFetcher::OneGoogleCallback> callback;
  one_google_bar_fetcher()->Fetch(callback.Get());

  base::RunLoop loop;
  EXPECT_CALL(callback,
              Run(OneGoogleBarFetcher::Status::FATAL_ERROR, Eq(base::nullopt)))
      .WillOnce(Quit(&loop));
  loop.Run();
}

TEST_F(OneGoogleBarFetcherImplTest, IncompleteJsonErrorIsFatal) {
  SetUpResponseWithData(R"json({"update": { "ogb": {
  "html": {},
  "page_hooks": {}
}}})json");

  base::MockCallback<OneGoogleBarFetcher::OneGoogleCallback> callback;
  one_google_bar_fetcher()->Fetch(callback.Get());

  base::RunLoop loop;
  EXPECT_CALL(callback,
              Run(OneGoogleBarFetcher::Status::FATAL_ERROR, Eq(base::nullopt)))
      .WillOnce(Quit(&loop));
  loop.Run();
}

TEST_F(OneGoogleBarFetcherImplTest, MirrorAccountConsistencyNotRequired) {
  SetUpResponseWithData(kMinimalValidResponse);

  // Trigger a request.
  base::MockCallback<OneGoogleBarFetcher::OneGoogleCallback> callback;
  one_google_bar_fetcher()->Fetch(callback.Get());

  base::RunLoop loop;
  EXPECT_CALL(callback, Run(_, _)).WillOnce(Quit(&loop));
  loop.Run();

#if defined(OS_CHROMEOS)
  // On Chrome OS, X-Chrome-Connected header is present, but
  // enable_account_consistency is set to false.
  std::string header_value;
  EXPECT_TRUE(last_request_headers().GetHeader(signin::kChromeConnectedHeader,
                                               &header_value));
  // mode = PROFILE_MODE_DEFAULT
  EXPECT_EQ("mode=0,enable_account_consistency=false", header_value);
#else
  // On not Chrome OS, the X-Chrome-Connected header must not be present.
  EXPECT_FALSE(
      last_request_headers().HasHeader(signin::kChromeConnectedHeader));
#endif
}

class OneGoogleBarFetcherImplWithMirrorAccountConsistencyTest
    : public OneGoogleBarFetcherImplTest {
 public:
  OneGoogleBarFetcherImplWithMirrorAccountConsistencyTest()
      : OneGoogleBarFetcherImplTest(true) {}
};

TEST_F(OneGoogleBarFetcherImplWithMirrorAccountConsistencyTest,
       MirrorAccountConsistencyRequired) {
  SetUpResponseWithData(kMinimalValidResponse);

  // Trigger a request.
  base::MockCallback<OneGoogleBarFetcher::OneGoogleCallback> callback;
  one_google_bar_fetcher()->Fetch(callback.Get());

  base::RunLoop loop;
  EXPECT_CALL(callback, Run(_, _)).WillOnce(Quit(&loop));
  loop.Run();

  // Make sure mirror account consistency is requested.
#if defined(OS_CHROMEOS)
  // On Chrome OS, X-Chrome-Connected header is present, and
  // enable_account_consistency is set to true.
  std::string header_value;
  EXPECT_TRUE(last_request_headers().GetHeader(signin::kChromeConnectedHeader,
                                               &header_value));
  // mode = PROFILE_MODE_INCOGNITO_DISABLED | PROFILE_MODE_ADD_ACCOUNT_DISABLED
  EXPECT_EQ("mode=3,enable_account_consistency=true", header_value);
#else
  // This is not a valid case (mirror account consistency can only be required
  // on Chrome OS). This ensures in this case nothing happens.
  EXPECT_FALSE(
      last_request_headers().HasHeader(signin::kChromeConnectedHeader));
#endif
}

class OneGoogleBarFetcherImplWithRelativeApiUrlOverrideTest
    : public OneGoogleBarFetcherImplTest {
 public:
  OneGoogleBarFetcherImplWithRelativeApiUrlOverrideTest()
      : OneGoogleBarFetcherImplTest(std::string("/testapi?q=a")) {}
};

TEST_F(OneGoogleBarFetcherImplWithRelativeApiUrlOverrideTest,
       RequestUrlRespectsOverride) {
  SetUpResponseWithData(kMinimalValidResponse);

  // Trigger a request.
  base::MockCallback<OneGoogleBarFetcher::OneGoogleCallback> callback;
  one_google_bar_fetcher()->Fetch(callback.Get());

  base::RunLoop loop;
  EXPECT_CALL(callback, Run(_, _)).WillOnce(Quit(&loop));
  loop.Run();

  // Make sure the request URL corresponds to the override, but also contains
  // the "hl=" query param.
  EXPECT_EQ("/testapi", last_request_url().path());
  std::string expected_query =
      base::StringPrintf("q=a&hl=%s&async=fixed:0", kApplicationLocale);
  EXPECT_EQ(expected_query, last_request_url().query());
}

class OneGoogleBarFetcherImplWithAbsoluteApiUrlOverrideTest
    : public OneGoogleBarFetcherImplTest {
 public:
  OneGoogleBarFetcherImplWithAbsoluteApiUrlOverrideTest()
      : OneGoogleBarFetcherImplTest(std::string("http://test.com/path?q=a")) {}
};

TEST_F(OneGoogleBarFetcherImplWithAbsoluteApiUrlOverrideTest,
       RequestUrlRespectsOverride) {
  SetUpResponseWithData(kMinimalValidResponse);

  // Trigger a request.
  base::MockCallback<OneGoogleBarFetcher::OneGoogleCallback> callback;
  one_google_bar_fetcher()->Fetch(callback.Get());

  base::RunLoop loop;
  EXPECT_CALL(callback, Run(_, _)).WillOnce(Quit(&loop));
  loop.Run();

  // Make sure the request URL corresponds to the override, but also contains
  // the "hl=" query param.
  GURL expected_url = GURL(base::StringPrintf(
      "http://test.com/path?q=a&hl=%s&async=fixed:0", kApplicationLocale));
  EXPECT_EQ(expected_url, last_request_url());
}
