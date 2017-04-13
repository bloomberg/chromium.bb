// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/one_google_bar/one_google_bar_fetcher_impl.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/search/one_google_bar/one_google_bar_data.h"
#include "components/google/core/browser/google_url_tracker.h"
#include "components/safe_json/testing_json_parser.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "google_apis/gaia/fake_oauth2_token_service_delegate.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Eq;
using testing::IsEmpty;
using testing::SaveArg;
using testing::StartsWith;

namespace {

const char kMinimalValidResponse[] = R"json({"oneGoogleBar": {
  "html": { "privateDoNotAccessOrElseSafeHtmlWrappedValue": "" },
  "pageHooks": {}
}})json";

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
  OneGoogleBarFetcherImplTest()
      : signin_client_(&pref_service_),
        signin_manager_(&signin_client_, &account_tracker_),
        task_runner_(new base::TestSimpleTaskRunner()),
        request_context_getter_(
            new net::TestURLRequestContextGetter(task_runner_)),
        token_service_(base::MakeUnique<FakeOAuth2TokenServiceDelegate>(
            request_context_getter_.get())),
        google_url_tracker_(base::MakeUnique<GoogleURLTrackerClientStub>(),
                            GoogleURLTracker::UNIT_TEST_MODE),
        one_google_bar_fetcher_(&signin_manager_,
                                &token_service_,
                                request_context_getter_.get(),
                                &google_url_tracker_) {
    SigninManagerBase::RegisterProfilePrefs(pref_service_.registry());
    SigninManagerBase::RegisterPrefs(pref_service_.registry());
  }

  void SignIn() {
    signin_manager_.SignIn("account");
    token_service_.GetDelegate()->UpdateCredentials("account", "refresh_token");
  }

  void IssueAccessToken() {
    token_service_.IssueAllTokensForAccount(
        "account", "access_token",
        base::Time::Now() + base::TimeDelta::FromHours(1));
  }

  void IssueAccessTokenError() {
    token_service_.IssueErrorForAllPendingRequestsForAccount(
        "account",
        GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE));
  }

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

  OneGoogleBarFetcherImpl* one_google_bar_fetcher() {
    return &one_google_bar_fetcher_;
  }

 private:
  // variations::AppendVariationHeaders and SafeJsonParser require a
  // ThreadTaskRunnerHandle to be set.
  base::MessageLoop message_loop_;

  safe_json::TestingJsonParser::ScopedFactoryOverride factory_override_;

  net::TestURLFetcherFactory url_fetcher_factory_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;

  TestSigninClient signin_client_;
  AccountTrackerService account_tracker_;
  FakeSigninManagerBase signin_manager_;

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_getter_;
  FakeProfileOAuth2TokenService token_service_;
  GoogleURLTracker google_url_tracker_;

  OneGoogleBarFetcherImpl one_google_bar_fetcher_;
};

TEST_F(OneGoogleBarFetcherImplTest, UnauthenticatedRequestReturns) {
  base::MockCallback<OneGoogleBarFetcher::OneGoogleCallback> callback;
  one_google_bar_fetcher()->Fetch(callback.Get());

  base::Optional<OneGoogleBarData> data;
  EXPECT_CALL(callback, Run(_)).WillOnce(SaveArg<0>(&data));
  RespondWithData(kMinimalValidResponse);

  EXPECT_TRUE(data.has_value());
}

TEST_F(OneGoogleBarFetcherImplTest, AuthenticatedRequestReturns) {
  SignIn();

  base::MockCallback<OneGoogleBarFetcher::OneGoogleCallback> callback;
  one_google_bar_fetcher()->Fetch(callback.Get());

  IssueAccessToken();

  base::Optional<OneGoogleBarData> data;
  EXPECT_CALL(callback, Run(_)).WillOnce(SaveArg<0>(&data));
  RespondWithData(kMinimalValidResponse);

  EXPECT_TRUE(data.has_value());
}

TEST_F(OneGoogleBarFetcherImplTest, UnauthenticatedRequestHasApiKey) {
  base::MockCallback<OneGoogleBarFetcher::OneGoogleCallback> callback;
  one_google_bar_fetcher()->Fetch(callback.Get());

  // The request should have an API key (as a query param).
  EXPECT_THAT(GetRunningURLFetcher()->GetOriginalURL().query(),
              StartsWith("key="));

  // But no "Authorization" header.
  net::HttpRequestHeaders headers;
  GetRunningURLFetcher()->GetExtraRequestHeaders(&headers);
  EXPECT_FALSE(headers.HasHeader("Authorization"));
}

TEST_F(OneGoogleBarFetcherImplTest, AuthenticatedRequestHasAuthHeader) {
  SignIn();

  base::MockCallback<OneGoogleBarFetcher::OneGoogleCallback> callback;
  one_google_bar_fetcher()->Fetch(callback.Get());

  IssueAccessToken();

  // The request should *not* have an API key (as a query param).
  EXPECT_THAT(GetRunningURLFetcher()->GetOriginalURL().query(), IsEmpty());

  // It should have an "Authorization" header.
  net::HttpRequestHeaders headers;
  GetRunningURLFetcher()->GetExtraRequestHeaders(&headers);
  EXPECT_TRUE(headers.HasHeader("Authorization"));
}

TEST_F(OneGoogleBarFetcherImplTest,
       AuthenticatedRequestFallsBackToUnauthenticated) {
  SignIn();

  base::MockCallback<OneGoogleBarFetcher::OneGoogleCallback> callback;
  one_google_bar_fetcher()->Fetch(callback.Get());

  IssueAccessTokenError();

  // The request should have fallen back to unauthenticated mode with an API key
  // (as a query param).
  EXPECT_THAT(GetRunningURLFetcher()->GetOriginalURL().query(),
              StartsWith("key="));

  // But no "Authorization" header.
  net::HttpRequestHeaders headers;
  GetRunningURLFetcher()->GetExtraRequestHeaders(&headers);
  EXPECT_FALSE(headers.HasHeader("Authorization"));
}

TEST_F(OneGoogleBarFetcherImplTest, HandlesResponsePreamble) {
  base::MockCallback<OneGoogleBarFetcher::OneGoogleCallback> callback;
  one_google_bar_fetcher()->Fetch(callback.Get());

  // The reponse may contain a ")]}'" prefix. The fetcher should ignore that
  // during parsing.
  base::Optional<OneGoogleBarData> data;
  EXPECT_CALL(callback, Run(_)).WillOnce(SaveArg<0>(&data));
  RespondWithData(std::string(")]}'") + kMinimalValidResponse);

  EXPECT_TRUE(data.has_value());
}

TEST_F(OneGoogleBarFetcherImplTest, ParsesFullResponse) {
  base::MockCallback<OneGoogleBarFetcher::OneGoogleCallback> callback;
  one_google_bar_fetcher()->Fetch(callback.Get());

  base::Optional<OneGoogleBarData> data;
  EXPECT_CALL(callback, Run(_)).WillOnce(SaveArg<0>(&data));
  RespondWithData(R"json({"oneGoogleBar": {
    "html": { "privateDoNotAccessOrElseSafeHtmlWrappedValue": "bar_html" },
    "pageHooks": {
      "inHeadScript": {
        "privateDoNotAccessOrElseSafeScriptWrappedValue": "in_head_script"
      },
      "inHeadStyle": {
        "privateDoNotAccessOrElseSafeStyleSheetWrappedValue": "in_head_style"
      },
      "afterBarScript": {
        "privateDoNotAccessOrElseSafeScriptWrappedValue": "after_bar_script"
      },
      "endOfBodyHtml": {
        "privateDoNotAccessOrElseSafeHtmlWrappedValue": "end_of_body_html"
      },
      "endOfBodyScript": {
        "privateDoNotAccessOrElseSafeScriptWrappedValue": "end_of_body_script"
      }
    }
  }})json");

  ASSERT_TRUE(data.has_value());
  EXPECT_THAT(data->bar_html, Eq("bar_html"));
  EXPECT_THAT(data->in_head_script, Eq("in_head_script"));
  EXPECT_THAT(data->in_head_style, Eq("in_head_style"));
  EXPECT_THAT(data->after_bar_script, Eq("after_bar_script"));
  EXPECT_THAT(data->end_of_body_html, Eq("end_of_body_html"));
  EXPECT_THAT(data->end_of_body_script, Eq("end_of_body_script"));
}

TEST_F(OneGoogleBarFetcherImplTest, CoalescesMultipleRequests) {
  // Trigger two requests.
  base::MockCallback<OneGoogleBarFetcher::OneGoogleCallback> first_callback;
  one_google_bar_fetcher()->Fetch(first_callback.Get());
  net::URLFetcher* first_fetcher = GetRunningURLFetcher();
  base::MockCallback<OneGoogleBarFetcher::OneGoogleCallback> second_callback;
  one_google_bar_fetcher()->Fetch(second_callback.Get());
  net::URLFetcher* second_fetcher = GetRunningURLFetcher();

  // Expect that only one fetcher handles both requests.
  EXPECT_THAT(first_fetcher, Eq(second_fetcher));

  // But both callbacks should get called.
  base::Optional<OneGoogleBarData> first_data;
  base::Optional<OneGoogleBarData> second_data;

  EXPECT_CALL(first_callback, Run(_)).WillOnce(SaveArg<0>(&first_data));
  EXPECT_CALL(second_callback, Run(_)).WillOnce(SaveArg<0>(&second_data));

  RespondWithData(kMinimalValidResponse);

  // Ensure that both requests received a response.
  EXPECT_TRUE(first_data.has_value());
  EXPECT_TRUE(second_data.has_value());
}
