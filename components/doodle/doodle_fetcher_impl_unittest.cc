// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/doodle/doodle_fetcher_impl.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/values.h"
#include "components/google/core/browser/google_switches.h"
#include "components/google/core/browser/google_url_tracker.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Eq;

namespace doodle {

namespace {

const char kDoodleConfigPath[] = "/async/ddljson";

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

std::string Resolve(const std::string& relative_url) {
  return GURL(GoogleURLTracker::kDefaultGoogleHomepage)
      .Resolve(relative_url)
      .spec();
}

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

class DoodleFetcherImplTest : public testing::Test {
 public:
  DoodleFetcherImplTest()
      : url_(GURL(GoogleURLTracker::kDefaultGoogleHomepage)),
        google_url_tracker_(base::MakeUnique<GoogleURLTrackerClientStub>(),
                            GoogleURLTracker::UNIT_TEST_MODE),
        doodle_fetcher_(
            new net::TestURLRequestContextGetter(message_loop_.task_runner()),
            &google_url_tracker_,
            base::Bind(ParseJson)) {}

  void RespondWithData(const std::string& data) {
    RespondToFetcherWithData(GetRunningFetcher(), data);
  }

  void RespondToFetcherWithData(net::TestURLFetcher* url_fetcher,
                                const std::string& data) {
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

  DoodleFetcherImpl::FinishedCallback CreateResponseSavingCallback(
      DoodleState* state_out,
      base::Optional<DoodleConfig>* config_out) {
    return base::BindOnce(
        [](DoodleState* state_out, base::Optional<DoodleConfig>* config_out,
           DoodleState state, const base::Optional<DoodleConfig>& config) {
          if (state_out) {
            *state_out = state;
          }
          if (config_out) {
            *config_out = config;
          }
        },
        state_out, config_out);
  }

  DoodleFetcherImpl* doodle_fetcher() { return &doodle_fetcher_; }

  GURL GetGoogleBaseURL() { return google_url_tracker_.google_url(); }

 private:
  base::MessageLoop message_loop_;
  GURL url_;
  net::TestURLFetcherFactory url_fetcher_factory_;
  GoogleURLTracker google_url_tracker_;
  DoodleFetcherImpl doodle_fetcher_;
};

TEST_F(DoodleFetcherImplTest, ReturnsFromFetchWithoutError) {
  DoodleState state(DoodleState::NO_DOODLE);
  base::Optional<DoodleConfig> response;

  doodle_fetcher()->FetchDoodle(
      CreateResponseSavingCallback(&state, &response));
  RespondWithData(R"json({"ddljson": {
        "time_to_live_ms":55000,
        "large_image": {"url":"/logos/doodles/2015/some.gif"}
      }})json");

  EXPECT_THAT(state, Eq(DoodleState::AVAILABLE));
  EXPECT_TRUE(response.has_value());
}

TEST_F(DoodleFetcherImplTest, ReturnsFrom404FetchWithError) {
  DoodleState state(DoodleState::NO_DOODLE);
  base::Optional<DoodleConfig> response;

  doodle_fetcher()->FetchDoodle(
      CreateResponseSavingCallback(&state, &response));
  RespondWithError(net::ERR_FILE_NOT_FOUND);

  EXPECT_THAT(state, Eq(DoodleState::DOWNLOAD_ERROR));
  EXPECT_FALSE(response.has_value());
}

TEST_F(DoodleFetcherImplTest, ReturnsErrorForInvalidJson) {
  DoodleState state(DoodleState::NO_DOODLE);
  base::Optional<DoodleConfig> response;

  doodle_fetcher()->FetchDoodle(
      CreateResponseSavingCallback(&state, &response));
  RespondWithData("}");

  EXPECT_THAT(state, Eq(DoodleState::PARSING_ERROR));
  EXPECT_FALSE(response.has_value());
}

TEST_F(DoodleFetcherImplTest, ReturnsErrorForIncompleteJson) {
  DoodleState state(DoodleState::NO_DOODLE);
  base::Optional<DoodleConfig> response;

  doodle_fetcher()->FetchDoodle(
      CreateResponseSavingCallback(&state, &response));
  RespondWithData("{}");

  EXPECT_THAT(state, Eq(DoodleState::PARSING_ERROR));
  EXPECT_FALSE(response.has_value());
}

TEST_F(DoodleFetcherImplTest, ResponseContainsValidBaseInformation) {
  DoodleState state(DoodleState::NO_DOODLE);
  base::Optional<DoodleConfig> response;

  doodle_fetcher()->FetchDoodle(
      CreateResponseSavingCallback(&state, &response));
  RespondWithData(R"json()]}'{
      "ddljson": {
        "alt_text":"Mouseover Text",
        "doodle_type":"SIMPLE",
        "interactive_html":"\u003cstyle\u003e\u003c\/style\u003e",
        "search_url":"/search?q\u003dtest",
        "target_url":"/search?q\u003dtest\u0026sa\u003dX\u0026ved\u003d0ahUKEw",
        "time_to_live_ms":55000,
        "large_image": {
          "url":"/logos/doodles/2015/new-years-eve-2015-59854387958251-hp.gif"
        }
      }})json");

  EXPECT_THAT(state, Eq(DoodleState::AVAILABLE));
  ASSERT_TRUE(response.has_value());
  DoodleConfig config = response.value();

  EXPECT_TRUE(config.search_url.is_valid());
  EXPECT_THAT(config.search_url, Eq(Resolve("/search?q\u003dtest")));
  EXPECT_TRUE(config.fullpage_interactive_url.is_empty());
  EXPECT_TRUE(config.target_url.is_valid());
  EXPECT_THAT(config.target_url,
              Eq(Resolve("/search?q\u003dtest\u0026sa\u003dX\u0026ved\u003d"
                         "0ahUKEw")));
  EXPECT_THAT(config.doodle_type, Eq(DoodleType::SIMPLE));
  EXPECT_THAT(config.alt_text, Eq("Mouseover Text"));
  EXPECT_THAT(config.interactive_html,
              Eq("\u003cstyle\u003e\u003c/style\u003e"));

  EXPECT_THAT(config.time_to_live,
              Eq(base::TimeDelta::FromMilliseconds(55000)));
}

TEST_F(DoodleFetcherImplTest, DoodleExpiresWithinThirtyDaysForTooLargeTTL) {
  DoodleState state(DoodleState::NO_DOODLE);
  base::Optional<DoodleConfig> response;

  doodle_fetcher()->FetchDoodle(
      CreateResponseSavingCallback(&state, &response));
  RespondWithData(R"json({"ddljson": {
      "time_to_live_ms":5184000000,
      "large_image": {"url":"/logos/doodles/2015/some.gif"}
    }})json");  // 60 days

  EXPECT_THAT(state, Eq(DoodleState::AVAILABLE));
  ASSERT_TRUE(response.has_value());
  EXPECT_THAT(response.value().time_to_live,
              Eq(base::TimeDelta::FromMilliseconds(30ul * 24 * 60 * 60 *
                                                   1000)));  // 30 days
}

TEST_F(DoodleFetcherImplTest, DoodleExpiresImmediatelyWithNegativeTTL) {
  DoodleState state(DoodleState::NO_DOODLE);
  base::Optional<DoodleConfig> response;

  doodle_fetcher()->FetchDoodle(
      CreateResponseSavingCallback(&state, &response));
  RespondWithData(R"json({"ddljson": {
      "time_to_live_ms":-1,
      "large_image": {"url":"/logos/doodles/2015/some.gif"}
    }})json");

  EXPECT_THAT(state, Eq(DoodleState::AVAILABLE));
  ASSERT_TRUE(response.has_value());
  EXPECT_THAT(response.value().time_to_live,
              Eq(base::TimeDelta::FromMilliseconds(0)));
}

TEST_F(DoodleFetcherImplTest, DoodleExpiresImmediatelyWithoutValidTTL) {
  DoodleState state(DoodleState::NO_DOODLE);
  base::Optional<DoodleConfig> response;

  doodle_fetcher()->FetchDoodle(
      CreateResponseSavingCallback(&state, &response));
  RespondWithData(R"json({"ddljson": {
        "large_image": {"url":"/logos/doodles/2015/some.gif"}
      }})json");

  EXPECT_THAT(state, Eq(DoodleState::AVAILABLE));
  ASSERT_TRUE(response.has_value());
  EXPECT_THAT(response.value().time_to_live,
              Eq(base::TimeDelta::FromMilliseconds(0)));
}

TEST_F(DoodleFetcherImplTest, ReturnsNoDoodleForMissingLargeImageUrl) {
  DoodleState state(DoodleState::AVAILABLE);
  base::Optional<DoodleConfig> response;

  doodle_fetcher()->FetchDoodle(
      CreateResponseSavingCallback(&state, &response));
  RespondWithData(R"json({"ddljson": {
        "time_to_live_ms":55000,
        "large_image": {}
      }})json");

  EXPECT_THAT(state, Eq(DoodleState::NO_DOODLE));
  EXPECT_FALSE(response.has_value());
}

TEST_F(DoodleFetcherImplTest, EmptyResponsesCausesNoDoodleState) {
  DoodleState state(DoodleState::AVAILABLE);
  base::Optional<DoodleConfig> response;

  doodle_fetcher()->FetchDoodle(
      CreateResponseSavingCallback(&state, &response));
  RespondWithData("{\"ddljson\":{}}");

  EXPECT_THAT(state, Eq(DoodleState::NO_DOODLE));
  EXPECT_FALSE(response.has_value());
}

TEST_F(DoodleFetcherImplTest, ResponseContainsExactlyTheSampleImages) {
  DoodleState state(DoodleState::NO_DOODLE);
  base::Optional<DoodleConfig> response;

  doodle_fetcher()->FetchDoodle(
      CreateResponseSavingCallback(&state, &response));
  RespondWithData(R"json()]}'{
      "ddljson": {
        "time_to_live_ms":55000,
        "large_image": {
          "height":225,
          "is_animated_gif":true,
          "is_cta":false,
          "url":"/logos/doodles/2015/new-years-eve-2015-59854387958251-hp.gif",
          "width":489
        },
        "large_cta_image": {
          "height":225,
          "is_animated_gif":true,
          "is_cta":true,
          "url":"/logos/doodles/2015/new-years-eve-2015-59854387958251-cta.gif",
          "width":489
        },
        "transparent_large_image": {
          "height":225,
          "is_animated_gif":false,
          "is_cta":false,
          "url":"/logos/doodles/2015/new-years-eve-2015-59854387958251-thp.png",
          "width":510
        }
      }})json");

  EXPECT_THAT(state, Eq(DoodleState::AVAILABLE));
  ASSERT_TRUE(response.has_value());
  DoodleConfig config = response.value();

  EXPECT_TRUE(config.transparent_large_image.url.is_valid());
  EXPECT_THAT(config.transparent_large_image.url.spec(),
              Eq(Resolve("/logos/doodles/2015/new-years-eve-2015-5985438795"
                         "8251-thp.png")));
  EXPECT_THAT(config.transparent_large_image.width, Eq(510));
  EXPECT_THAT(config.transparent_large_image.height, Eq(225));
  EXPECT_FALSE(config.transparent_large_image.is_animated_gif);
  EXPECT_FALSE(config.transparent_large_image.is_cta);

  EXPECT_TRUE(config.large_image.url.is_valid());
  EXPECT_THAT(config.large_image.url.spec(),
              Eq(Resolve("/logos/doodles/2015/new-years-eve-2015-5985438795"
                         "8251-hp.gif")));
  EXPECT_THAT(config.large_image.width, Eq(489));
  EXPECT_THAT(config.large_image.height, Eq(225));
  EXPECT_TRUE(config.large_image.is_animated_gif);
  EXPECT_FALSE(config.large_image.is_cta);

  EXPECT_TRUE(config.large_cta_image.url.is_valid());
  EXPECT_THAT(config.large_cta_image.url.spec(),
              Eq(Resolve("/logos/doodles/2015/new-years-eve-2015-5985438795"
                         "8251-cta.gif")));
  EXPECT_THAT(config.large_cta_image.width, Eq(489));
  EXPECT_THAT(config.large_cta_image.height, Eq(225));
  EXPECT_TRUE(config.large_cta_image.is_animated_gif);
  EXPECT_TRUE(config.large_cta_image.is_cta);
}

TEST_F(DoodleFetcherImplTest, RespondsToMultipleRequestsWithSameFetcher) {
  DoodleState state1(DoodleState::NO_DOODLE);
  DoodleState state2(DoodleState::NO_DOODLE);
  base::Optional<DoodleConfig> response1;
  base::Optional<DoodleConfig> response2;

  // Trigger two requests.
  doodle_fetcher()->FetchDoodle(
      CreateResponseSavingCallback(&state1, &response1));
  net::URLFetcher* first_created_fetcher = GetRunningFetcher();
  doodle_fetcher()->FetchDoodle(
      CreateResponseSavingCallback(&state2, &response2));
  net::URLFetcher* second_created_fetcher = GetRunningFetcher();

  // Expect that only one fetcher handles both requests.
  EXPECT_THAT(first_created_fetcher, Eq(second_created_fetcher));

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
  doodle_fetcher()->FetchDoodle(
      CreateResponseSavingCallback(/*state=*/nullptr, /*response=*/nullptr));

  EXPECT_THAT(GetRunningFetcher()->GetOriginalURL(),
              Eq(GetGoogleBaseURL().Resolve(kDoodleConfigPath)));
}

TEST_F(DoodleFetcherImplTest, OverridesBaseUrlWithCommandLineArgument) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kGoogleBaseURL, "http://www.google.kz");

  doodle_fetcher()->FetchDoodle(
      CreateResponseSavingCallback(/*state=*/nullptr, /*response=*/nullptr));

  EXPECT_THAT(GetRunningFetcher()->GetOriginalURL(),
              Eq(GURL("http://www.google.kz").Resolve(kDoodleConfigPath)));
}

}  // namespace doodle
