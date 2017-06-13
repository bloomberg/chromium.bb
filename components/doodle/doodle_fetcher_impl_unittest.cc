// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/doodle/doodle_fetcher_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/google/core/browser/google_switches.h"
#include "components/google/core/browser/google_url_tracker.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::DoAll;
using testing::Eq;
using testing::SaveArg;

namespace doodle {

namespace {

const char kDoodleConfigPath[] = "/async/ddljson?async=ntp:1,graybg:1";
const char kDoodleConfigPathNoGrayBg[] = "/async/ddljson?async=ntp:1,graybg:0";

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

void ParseJson(
    const std::string& json,
    const base::Callback<void(std::unique_ptr<base::Value> json)>& success,
    const base::Callback<void(const std::string&)>& error) {
  base::JSONReader json_reader;
  std::unique_ptr<base::Value> value = json_reader.ReadToValue(json);
  if (value) {
    success.Run(std::move(value));
  } else {
    error.Run(json_reader.GetErrorMessage());
  }
}

}  // namespace

class DoodleFetcherImplTestBase : public testing::Test {
 public:
  DoodleFetcherImplTestBase(bool gray_background,
                            const base::Optional<std::string>& override_url)
      : google_url_tracker_(base::MakeUnique<GoogleURLTrackerClientStub>(),
                            GoogleURLTracker::UNIT_TEST_MODE),
        doodle_fetcher_(
            new net::TestURLRequestContextGetter(message_loop_.task_runner()),
            &google_url_tracker_,
            base::Bind(ParseJson),
            gray_background,
            override_url) {}

  void RespondWithData(const std::string& data) {
    net::TestURLFetcher* url_fetcher = GetRunningFetcher();
    url_fetcher->set_status(net::URLRequestStatus());
    url_fetcher->set_response_code(net::HTTP_OK);
    url_fetcher->SetResponseString(data);
    // Call the URLFetcher delegate to continue the test.
    url_fetcher->delegate()->OnURLFetchComplete(url_fetcher);
  }

  void RespondWithError(int error_code) {
    net::TestURLFetcher* url_fetcher = GetRunningFetcher();
    url_fetcher->set_status(net::URLRequestStatus::FromError(error_code));
    url_fetcher->SetResponseString("");
    // Call the URLFetcher delegate to continue the test.
    url_fetcher->delegate()->OnURLFetchComplete(url_fetcher);
  }

  net::TestURLFetcher* GetRunningFetcher() {
    // All created TestURLFetchers have ID 0 by default.
    net::TestURLFetcher* url_fetcher = url_fetcher_factory_.GetFetcherByID(0);
    DCHECK(url_fetcher);
    return url_fetcher;
  }

  DoodleFetcherImpl* doodle_fetcher() { return &doodle_fetcher_; }

  GURL GetGoogleBaseURL() { return google_url_tracker_.google_url(); }

  GURL Resolve(const std::string& relative_url) {
    return GetGoogleBaseURL().Resolve(relative_url);
  }

 private:
  base::MessageLoop message_loop_;
  net::TestURLFetcherFactory url_fetcher_factory_;
  GoogleURLTracker google_url_tracker_;
  DoodleFetcherImpl doodle_fetcher_;
};

class DoodleFetcherImplTest : public DoodleFetcherImplTestBase {
 public:
  DoodleFetcherImplTest()
      : DoodleFetcherImplTestBase(/*gray_background=*/true,
                                  /*override_url=*/base::nullopt) {}
};

TEST_F(DoodleFetcherImplTest, ReturnsFromFetchWithoutError) {
  base::MockCallback<DoodleFetcherImpl::FinishedCallback> callback;
  doodle_fetcher()->FetchDoodle(callback.Get());

  DoodleState state = DoodleState::NO_DOODLE;
  base::Optional<DoodleConfig> response;
  EXPECT_CALL(callback, Run(_, _, _))
      .WillOnce(DoAll(SaveArg<0>(&state), SaveArg<2>(&response)));
  RespondWithData(R"json({"ddljson": {
        "time_to_live_ms":55000,
        "large_image": {"url":"/logos/doodles/2015/some.gif"}
      }})json");

  EXPECT_THAT(state, Eq(DoodleState::AVAILABLE));
  EXPECT_TRUE(response.has_value());
}

TEST_F(DoodleFetcherImplTest, ReturnsFrom404FetchWithError) {
  base::MockCallback<DoodleFetcherImpl::FinishedCallback> callback;
  doodle_fetcher()->FetchDoodle(callback.Get());

  DoodleState state = DoodleState::NO_DOODLE;
  base::Optional<DoodleConfig> response;
  EXPECT_CALL(callback, Run(_, _, _))
      .WillOnce(DoAll(SaveArg<0>(&state), SaveArg<2>(&response)));
  RespondWithError(net::ERR_FILE_NOT_FOUND);

  EXPECT_THAT(state, Eq(DoodleState::DOWNLOAD_ERROR));
  EXPECT_FALSE(response.has_value());
}

TEST_F(DoodleFetcherImplTest, ReturnsErrorForInvalidJson) {
  base::MockCallback<DoodleFetcherImpl::FinishedCallback> callback;
  doodle_fetcher()->FetchDoodle(callback.Get());

  DoodleState state = DoodleState::NO_DOODLE;
  base::Optional<DoodleConfig> response;
  EXPECT_CALL(callback, Run(_, _, _))
      .WillOnce(DoAll(SaveArg<0>(&state), SaveArg<2>(&response)));
  RespondWithData("}");

  EXPECT_THAT(state, Eq(DoodleState::PARSING_ERROR));
  EXPECT_FALSE(response.has_value());
}

TEST_F(DoodleFetcherImplTest, ReturnsErrorForIncompleteJson) {
  base::MockCallback<DoodleFetcherImpl::FinishedCallback> callback;
  doodle_fetcher()->FetchDoodle(callback.Get());

  DoodleState state = DoodleState::NO_DOODLE;
  base::Optional<DoodleConfig> response;
  EXPECT_CALL(callback, Run(_, _, _))
      .WillOnce(DoAll(SaveArg<0>(&state), SaveArg<2>(&response)));
  RespondWithData("{}");

  EXPECT_THAT(state, Eq(DoodleState::PARSING_ERROR));
  EXPECT_FALSE(response.has_value());
}

TEST_F(DoodleFetcherImplTest, ResponseContainsValidBaseInformation) {
  base::MockCallback<DoodleFetcherImpl::FinishedCallback> callback;
  doodle_fetcher()->FetchDoodle(callback.Get());

  DoodleState state = DoodleState::NO_DOODLE;
  base::TimeDelta time_to_live;
  base::Optional<DoodleConfig> response;
  EXPECT_CALL(callback, Run(_, _, _))
      .WillOnce(DoAll(SaveArg<0>(&state), SaveArg<1>(&time_to_live),
                      SaveArg<2>(&response)));
  RespondWithData(R"json()]}'{
      "ddljson": {
        "alt_text":"Mouseover Text",
        "doodle_type":"SIMPLE",
        "target_url":"/search?q\u003dtest\u0026sa\u003dX\u0026ved\u003d0ahUKEw",
        "time_to_live_ms":55000,
        "large_image": {
          "url":"/logos/doodles/2015/new-years-eve-2015-59854387958251-hp.gif"
        }
      }})json");

  EXPECT_THAT(state, Eq(DoodleState::AVAILABLE));
  ASSERT_TRUE(response.has_value());
  DoodleConfig config = response.value();

  EXPECT_TRUE(config.target_url.is_valid());
  EXPECT_THAT(config.target_url,
              Eq(Resolve("/search?q\u003dtest\u0026sa\u003dX\u0026ved\u003d"
                         "0ahUKEw")));
  EXPECT_THAT(config.doodle_type, Eq(DoodleType::SIMPLE));
  EXPECT_THAT(config.alt_text, Eq("Mouseover Text"));

  EXPECT_FALSE(config.large_cta_image.has_value());

  EXPECT_THAT(time_to_live, Eq(base::TimeDelta::FromMilliseconds(55000)));
}

TEST_F(DoodleFetcherImplTest, DoodleExpiresImmediatelyWithNegativeTTL) {
  base::MockCallback<DoodleFetcherImpl::FinishedCallback> callback;
  doodle_fetcher()->FetchDoodle(callback.Get());

  DoodleState state = DoodleState::NO_DOODLE;
  base::TimeDelta time_to_live;
  base::Optional<DoodleConfig> response;
  EXPECT_CALL(callback, Run(_, _, _))
      .WillOnce(DoAll(SaveArg<0>(&state), SaveArg<1>(&time_to_live),
                      SaveArg<2>(&response)));
  RespondWithData(R"json({"ddljson": {
      "time_to_live_ms":-1,
      "large_image": {"url":"/logos/doodles/2015/some.gif"}
    }})json");

  EXPECT_THAT(state, Eq(DoodleState::AVAILABLE));
  EXPECT_TRUE(response.has_value());
  EXPECT_THAT(time_to_live, Eq(base::TimeDelta::FromMilliseconds(0)));
}

TEST_F(DoodleFetcherImplTest, DoodleExpiresImmediatelyWithoutValidTTL) {
  base::MockCallback<DoodleFetcherImpl::FinishedCallback> callback;
  doodle_fetcher()->FetchDoodle(callback.Get());

  DoodleState state = DoodleState::NO_DOODLE;
  base::TimeDelta time_to_live;
  base::Optional<DoodleConfig> response;
  EXPECT_CALL(callback, Run(_, _, _))
      .WillOnce(DoAll(SaveArg<0>(&state), SaveArg<1>(&time_to_live),
                      SaveArg<2>(&response)));
  RespondWithData(R"json({"ddljson": {
        "large_image": {"url":"/logos/doodles/2015/some.gif"}
      }})json");

  EXPECT_THAT(state, Eq(DoodleState::AVAILABLE));
  EXPECT_TRUE(response.has_value());
  EXPECT_THAT(time_to_live, Eq(base::TimeDelta::FromMilliseconds(0)));
}

TEST_F(DoodleFetcherImplTest, ReturnsNoDoodleForMissingLargeImageUrl) {
  base::MockCallback<DoodleFetcherImpl::FinishedCallback> callback;
  doodle_fetcher()->FetchDoodle(callback.Get());

  DoodleState state = DoodleState::AVAILABLE;
  base::Optional<DoodleConfig> response;
  EXPECT_CALL(callback, Run(_, _, _))
      .WillOnce(DoAll(SaveArg<0>(&state), SaveArg<2>(&response)));
  RespondWithData(R"json({"ddljson": {
        "time_to_live_ms":55000,
        "large_image": {}
      }})json");

  EXPECT_THAT(state, Eq(DoodleState::NO_DOODLE));
  EXPECT_FALSE(response.has_value());
}

TEST_F(DoodleFetcherImplTest, EmptyResponsesCausesNoDoodleState) {
  base::MockCallback<DoodleFetcherImpl::FinishedCallback> callback;
  doodle_fetcher()->FetchDoodle(callback.Get());

  DoodleState state = DoodleState::AVAILABLE;
  base::Optional<DoodleConfig> response;
  EXPECT_CALL(callback, Run(_, _, _))
      .WillOnce(DoAll(SaveArg<0>(&state), SaveArg<2>(&response)));
  RespondWithData("{\"ddljson\":{}}");

  EXPECT_THAT(state, Eq(DoodleState::NO_DOODLE));
  EXPECT_FALSE(response.has_value());
}

TEST_F(DoodleFetcherImplTest, ResponseContainsExactlyTheSampleImages) {
  base::MockCallback<DoodleFetcherImpl::FinishedCallback> callback;
  doodle_fetcher()->FetchDoodle(callback.Get());

  DoodleState state = DoodleState::NO_DOODLE;
  base::Optional<DoodleConfig> response;
  EXPECT_CALL(callback, Run(_, _, _))
      .WillOnce(DoAll(SaveArg<0>(&state), SaveArg<2>(&response)));
  RespondWithData(R"json()]}'{
      "ddljson": {
        "time_to_live_ms":55000,
        "large_image": {
          "height":225,
          "url":"/logos/doodles/2015/new-years-eve-2015-59854387958251-hp.gif",
          "width":489
        },
        "large_cta_image": {
          "height":225,
          "url":"/logos/doodles/2015/new-years-eve-2015-59854387958251-cta.gif",
          "width":489
        }
      }})json");

  EXPECT_THAT(state, Eq(DoodleState::AVAILABLE));
  ASSERT_TRUE(response.has_value());
  DoodleConfig config = response.value();

  EXPECT_TRUE(config.large_image.url.is_valid());
  EXPECT_THAT(config.large_image.url,
              Eq(Resolve("/logos/doodles/2015/new-years-eve-2015-5985438795"
                         "8251-hp.gif")));

  ASSERT_TRUE(config.large_cta_image.has_value());
  EXPECT_TRUE(config.large_cta_image->url.is_valid());
  EXPECT_THAT(config.large_cta_image->url,
              Eq(Resolve("/logos/doodles/2015/new-years-eve-2015-5985438795"
                         "8251-cta.gif")));
}

TEST_F(DoodleFetcherImplTest, RespondsToMultipleRequestsWithSameFetcher) {
  // Trigger two requests.
  base::MockCallback<DoodleFetcherImpl::FinishedCallback> callback1;
  doodle_fetcher()->FetchDoodle(callback1.Get());
  net::URLFetcher* first_created_fetcher = GetRunningFetcher();
  base::MockCallback<DoodleFetcherImpl::FinishedCallback> callback2;
  doodle_fetcher()->FetchDoodle(callback2.Get());
  net::URLFetcher* second_created_fetcher = GetRunningFetcher();

  // Expect that only one fetcher handles both requests.
  EXPECT_THAT(first_created_fetcher, Eq(second_created_fetcher));

  // But both callbacks should get called.
  DoodleState state1 = DoodleState::NO_DOODLE;
  DoodleState state2 = DoodleState::NO_DOODLE;
  base::Optional<DoodleConfig> response1;
  base::Optional<DoodleConfig> response2;

  EXPECT_CALL(callback1, Run(_, _, _))
      .WillOnce(DoAll(SaveArg<0>(&state1), SaveArg<2>(&response1)));
  EXPECT_CALL(callback2, Run(_, _, _))
      .WillOnce(DoAll(SaveArg<0>(&state2), SaveArg<2>(&response2)));

  RespondWithData(R"json({"ddljson": {
        "time_to_live_ms":55000,
        "large_image": {"url":"/logos/doodles/2015/some.gif"}
      }})json");

  // Ensure that both requests received a response.
  EXPECT_THAT(state1, Eq(DoodleState::AVAILABLE));
  EXPECT_TRUE(response1.has_value());
  EXPECT_THAT(state2, Eq(DoodleState::AVAILABLE));
  EXPECT_TRUE(response2.has_value());
}

TEST_F(DoodleFetcherImplTest, ReceivesBaseUrlFromTracker) {
  base::MockCallback<DoodleFetcherImpl::FinishedCallback> callback;
  doodle_fetcher()->FetchDoodle(callback.Get());

  // TODO(treib,fhorschig): This doesn't really test anything useful, since the
  // Google base URL is the default anyway. Find a way to set the base URL in
  // the tracker.
  EXPECT_THAT(GetRunningFetcher()->GetOriginalURL(),
              Eq(Resolve(kDoodleConfigPath)));
}

TEST_F(DoodleFetcherImplTest, OverridesBaseUrlWithCommandLineArgument) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kGoogleBaseURL, "http://www.google.kz");

  base::MockCallback<DoodleFetcherImpl::FinishedCallback> callback;
  doodle_fetcher()->FetchDoodle(callback.Get());

  EXPECT_THAT(GetRunningFetcher()->GetOriginalURL(),
              Eq(GURL("http://www.google.kz").Resolve(kDoodleConfigPath)));
}

class DoodleFetcherImplNoGrayBgTest : public DoodleFetcherImplTestBase {
 public:
  DoodleFetcherImplNoGrayBgTest()
      : DoodleFetcherImplTestBase(/*gray_background=*/false,
                                  /*override_url=*/base::nullopt) {}
};

TEST_F(DoodleFetcherImplNoGrayBgTest, PassesNoGrayBgParam) {
  base::MockCallback<DoodleFetcherImpl::FinishedCallback> callback;
  doodle_fetcher()->FetchDoodle(callback.Get());

  EXPECT_THAT(GetRunningFetcher()->GetOriginalURL(),
              Eq(Resolve(kDoodleConfigPathNoGrayBg)));
}

class DoodleFetcherImplRelativeOverrideUrlTest
    : public DoodleFetcherImplTestBase {
 public:
  DoodleFetcherImplRelativeOverrideUrlTest()
      : DoodleFetcherImplTestBase(/*gray_background=*/false,
                                  /*override_url=*/std::string("/different")) {}
};

TEST_F(DoodleFetcherImplRelativeOverrideUrlTest, OverridesWithRelativeUrl) {
  base::MockCallback<DoodleFetcherImpl::FinishedCallback> callback;
  doodle_fetcher()->FetchDoodle(callback.Get());

  EXPECT_THAT(GetRunningFetcher()->GetOriginalURL(), Eq(Resolve("/different")));
}

class DoodleFetcherImplAbsoluteOverrideUrlTest
    : public DoodleFetcherImplTestBase {
 public:
  DoodleFetcherImplAbsoluteOverrideUrlTest()
      : DoodleFetcherImplTestBase(
            /*gray_background=*/false,
            /*override_url=*/std::string("http://host.com/ddl")) {}
};

TEST_F(DoodleFetcherImplAbsoluteOverrideUrlTest, OverridesWithAbsoluteUrl) {
  base::MockCallback<DoodleFetcherImpl::FinishedCallback> callback;
  doodle_fetcher()->FetchDoodle(callback.Get());

  EXPECT_THAT(GetRunningFetcher()->GetOriginalURL(),
              Eq(GURL("http://host.com/ddl")));
}

}  // namespace doodle
