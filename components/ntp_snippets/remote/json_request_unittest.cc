// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/json_request.h"

#include <set>
#include <utility>

#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/ntp_snippets_constants.h"
#include "components/ntp_snippets/remote/request_params.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/variations_params_manager.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ntp_snippets {

namespace internal {

namespace {

using testing::_;
using testing::Eq;
using testing::Not;
using testing::NotNull;
using testing::StrEq;

MATCHER_P(EqualsJSON, json, "equals JSON") {
  std::unique_ptr<base::Value> expected = base::JSONReader::Read(json);
  if (!expected) {
    *result_listener << "INTERNAL ERROR: couldn't parse expected JSON";
    return false;
  }

  std::string err_msg;
  int err_line, err_col;
  std::unique_ptr<base::Value> actual = base::JSONReader::ReadAndReturnError(
      arg, base::JSON_PARSE_RFC, nullptr, &err_msg, &err_line, &err_col);
  if (!actual) {
    *result_listener << "input:" << err_line << ":" << err_col << ": "
                     << "parse error: " << err_msg;
    return false;
  }
  return base::Value::Equals(actual.get(), expected.get());
}

}  // namespace

class JsonRequestTest : public testing::Test {
 public:
  JsonRequestTest()
      : params_manager_(
            ntp_snippets::kStudyName,
            {{"send_top_languages", "true"}, {"send_user_class", "true"}},
            {ntp_snippets::kArticleSuggestionsFeature.name}),
        pref_service_(base::MakeUnique<TestingPrefServiceSimple>()),
        mock_task_runner_(new base::TestMockTimeTaskRunner()),
        clock_(mock_task_runner_->GetMockClock()),
        request_context_getter_(
            new net::TestURLRequestContextGetter(mock_task_runner_.get())) {
    translate::LanguageModel::RegisterProfilePrefs(pref_service_->registry());
  }

  std::unique_ptr<translate::LanguageModel> MakeLanguageModel(
      const std::set<std::string>& codes) {
    std::unique_ptr<translate::LanguageModel> language_model =
        base::MakeUnique<translate::LanguageModel>(pref_service_.get());
    // There must be at least 10 visits before the top languages are defined.
    for (int i = 0; i < 10; i++) {
      for (const std::string& code : codes) {
        language_model->OnPageVisited(code);
      }
    }
    return language_model;
  }

  JsonRequest::Builder CreateMinimalBuilder() {
    JsonRequest::Builder builder;
    builder.SetUrl(GURL("http://valid-url.test"))
        .SetClock(clock_.get())
        .SetUrlRequestContextGetter(request_context_getter_.get());
    return builder;
  }

 private:
  variations::testing::VariationParamsManager params_manager_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  scoped_refptr<base::TestMockTimeTaskRunner> mock_task_runner_;
  std::unique_ptr<base::Clock> clock_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_getter_;
  net::TestURLFetcherFactory fetcher_factory_;

  DISALLOW_COPY_AND_ASSIGN(JsonRequestTest);
};

TEST_F(JsonRequestTest, BuildRequestAuthenticated) {
  JsonRequest::Builder builder = CreateMinimalBuilder();
  RequestParams params;
  params.excluded_ids = {"1234567890"};
  params.count_to_fetch = 25;
  params.interactive_request = false;
  builder.SetParams(params)
      .SetUrl(GURL("http://valid-url.test"))
      .SetUrl(GURL("http://valid-url.test"))
      .SetAuthentication("0BFUSGAIA", "headerstuff")
      .SetUserClassForTesting("ACTIVE_NTP_USER")
      .SetFetchAPI(FetchAPI::CHROME_READER_API)
      .Build();

  EXPECT_THAT(builder.PreviewRequestHeadersForTesting(),
              StrEq("Content-Type: application/json; charset=UTF-8\r\n"
                    "Authorization: headerstuff\r\n"
                    "\r\n"));
  EXPECT_THAT(builder.PreviewRequestBodyForTesting(),
              EqualsJSON("{"
                         "  \"response_detail_level\": \"STANDARD\","
                         "  \"obfuscated_gaia_id\": \"0BFUSGAIA\","
                         "  \"advanced_options\": {"
                         "    \"local_scoring_params\": {"
                         "      \"content_restricts\": ["
                         "        {"
                         "          \"type\": \"METADATA\","
                         "          \"value\": \"TITLE\""
                         "        },"
                         "        {"
                         "          \"type\": \"METADATA\","
                         "          \"value\": \"SNIPPET\""
                         "        },"
                         "        {"
                         "          \"type\": \"METADATA\","
                         "          \"value\": \"THUMBNAIL\""
                         "        }"
                         "      ]"
                         "    },"
                         "    \"global_scoring_params\": {"
                         "      \"num_to_return\": 25,"
                         "      \"sort_type\": 1"
                         "    }"
                         "  }"
                         "}"));

  builder.SetFetchAPI(FetchAPI::CHROME_CONTENT_SUGGESTIONS_API);

  EXPECT_THAT(builder.PreviewRequestBodyForTesting(),
              EqualsJSON("{"
                         "  \"priority\": \"BACKGROUND_PREFETCH\","
                         "  \"excludedSuggestionIds\": ["
                         "    \"1234567890\""
                         "  ],"
                         "  \"userActivenessClass\": \"ACTIVE_NTP_USER\""
                         "}"));
}

TEST_F(JsonRequestTest, BuildRequestUnauthenticated) {
  JsonRequest::Builder builder;
  RequestParams params;
  params.interactive_request = true;
  params.count_to_fetch = 10;
  builder.SetParams(params)
      .SetUserClassForTesting("ACTIVE_NTP_USER")
      .SetFetchAPI(FetchAPI::CHROME_READER_API);

  EXPECT_THAT(builder.PreviewRequestHeadersForTesting(),
              StrEq("Content-Type: application/json; charset=UTF-8\r\n"
                    "\r\n"));
  EXPECT_THAT(builder.PreviewRequestBodyForTesting(),
              EqualsJSON("{"
                         "  \"response_detail_level\": \"STANDARD\","
                         "  \"advanced_options\": {"
                         "    \"local_scoring_params\": {"
                         "      \"content_restricts\": ["
                         "        {"
                         "          \"type\": \"METADATA\","
                         "          \"value\": \"TITLE\""
                         "        },"
                         "        {"
                         "          \"type\": \"METADATA\","
                         "          \"value\": \"SNIPPET\""
                         "        },"
                         "        {"
                         "          \"type\": \"METADATA\","
                         "          \"value\": \"THUMBNAIL\""
                         "        }"
                         "      ]"
                         "    },"
                         "    \"global_scoring_params\": {"
                         "      \"num_to_return\": 10,"
                         "      \"sort_type\": 1"
                         "    }"
                         "  }"
                         "}"));

  builder.SetFetchAPI(FetchAPI::CHROME_CONTENT_SUGGESTIONS_API);
  EXPECT_THAT(builder.PreviewRequestBodyForTesting(),
              EqualsJSON("{"
                         "  \"priority\": \"USER_ACTION\","
                         "  \"excludedSuggestionIds\": [],"
                         "  \"userActivenessClass\": \"ACTIVE_NTP_USER\""
                         "}"));
}

TEST_F(JsonRequestTest, BuildRequestExcludedIds) {
  JsonRequest::Builder builder;
  RequestParams params;
  params.interactive_request = false;
  for (int i = 0; i < 200; ++i) {
    params.excluded_ids.insert(base::StringPrintf("%03d", i));
  }
  builder.SetParams(params)
      .SetUserClassForTesting("ACTIVE_NTP_USER")
      .SetFetchAPI(FetchAPI::CHROME_CONTENT_SUGGESTIONS_API);

  EXPECT_THAT(builder.PreviewRequestBodyForTesting(),
              EqualsJSON("{"
                         "  \"priority\": \"BACKGROUND_PREFETCH\","
                         "  \"excludedSuggestionIds\": ["
                         "    \"000\", \"001\", \"002\", \"003\", \"004\","
                         "    \"005\", \"006\", \"007\", \"008\", \"009\","
                         "    \"010\", \"011\", \"012\", \"013\", \"014\","
                         "    \"015\", \"016\", \"017\", \"018\", \"019\","
                         "    \"020\", \"021\", \"022\", \"023\", \"024\","
                         "    \"025\", \"026\", \"027\", \"028\", \"029\","
                         "    \"030\", \"031\", \"032\", \"033\", \"034\","
                         "    \"035\", \"036\", \"037\", \"038\", \"039\","
                         "    \"040\", \"041\", \"042\", \"043\", \"044\","
                         "    \"045\", \"046\", \"047\", \"048\", \"049\","
                         "    \"050\", \"051\", \"052\", \"053\", \"054\","
                         "    \"055\", \"056\", \"057\", \"058\", \"059\","
                         "    \"060\", \"061\", \"062\", \"063\", \"064\","
                         "    \"065\", \"066\", \"067\", \"068\", \"069\","
                         "    \"070\", \"071\", \"072\", \"073\", \"074\","
                         "    \"075\", \"076\", \"077\", \"078\", \"079\","
                         "    \"080\", \"081\", \"082\", \"083\", \"084\","
                         "    \"085\", \"086\", \"087\", \"088\", \"089\","
                         "    \"090\", \"091\", \"092\", \"093\", \"094\","
                         "    \"095\", \"096\", \"097\", \"098\", \"099\""
                         // Truncated to 100 entries. Currently, they happen to
                         // be those lexically first.
                         "  ],"
                         "  \"userActivenessClass\": \"ACTIVE_NTP_USER\""
                         "}"));
}

TEST_F(JsonRequestTest, BuildRequestNoUserClass) {
  JsonRequest::Builder builder;
  RequestParams params;
  params.interactive_request = false;
  builder.SetParams(params)
      .SetFetchAPI(FetchAPI::CHROME_CONTENT_SUGGESTIONS_API);

  EXPECT_THAT(builder.PreviewRequestBodyForTesting(),
              EqualsJSON("{"
                         "  \"priority\": \"BACKGROUND_PREFETCH\","
                         "  \"excludedSuggestionIds\": []"
                         "}"));
}

TEST_F(JsonRequestTest, BuildRequestWithTwoLanguages) {
  JsonRequest::Builder builder;
  std::unique_ptr<translate::LanguageModel> language_model =
      MakeLanguageModel({"de", "en"});
  RequestParams params;
  params.interactive_request = true;
  params.language_code = "en";
  builder.SetParams(params)
      .SetLanguageModel(language_model.get())
      .SetFetchAPI(FetchAPI::CHROME_CONTENT_SUGGESTIONS_API);

  EXPECT_THAT(builder.PreviewRequestBodyForTesting(),
              EqualsJSON("{"
                         "  \"priority\": \"USER_ACTION\","
                         "  \"uiLanguage\": \"en\","
                         "  \"excludedSuggestionIds\": [],"
                         "  \"topLanguages\": ["
                         "    {"
                         "      \"language\" : \"en\","
                         "      \"frequency\" : 0.5"
                         "    },"
                         "    {"
                         "      \"language\" : \"de\","
                         "      \"frequency\" : 0.5"
                         "    }"
                         "  ]"
                         "}"));
}

TEST_F(JsonRequestTest, BuildRequestWithUILanguageOnly) {
  JsonRequest::Builder builder;
  std::unique_ptr<translate::LanguageModel> language_model =
      MakeLanguageModel({"en"});
  RequestParams params;
  params.interactive_request = true;
  params.language_code = "en";
  builder.SetParams(params)
      .SetLanguageModel(language_model.get())
      .SetFetchAPI(FetchAPI::CHROME_CONTENT_SUGGESTIONS_API);

  EXPECT_THAT(builder.PreviewRequestBodyForTesting(),
              EqualsJSON("{"
                         "  \"priority\": \"USER_ACTION\","
                         "  \"uiLanguage\": \"en\","
                         "  \"excludedSuggestionIds\": [],"
                         "  \"topLanguages\": [{"
                         "    \"language\" : \"en\","
                         "    \"frequency\" : 1.0"
                         "  }]"
                         "}"));
}

}  // namespace internal

}  // namespace ntp_snippets
