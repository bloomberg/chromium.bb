// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/dial/dial_app_discovery_service.h"

#include "base/strings/stringprintf.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/media/router/discovery/dial/dial_app_info_fetcher.h"
#include "chrome/browser/media/router/discovery/dial/parsed_dial_device_description.h"
#include "chrome/browser/media/router/discovery/dial/safe_dial_app_info_parser.h"
#include "chrome/browser/media/router/test/test_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SaveArg;

namespace media_router {

namespace {

constexpr char kYouTubeName[] = "YouTube";
constexpr char kNetflixName[] = "Netflix";

std::unique_ptr<ParsedDialAppInfo> CreateParsedDialAppInfo(
    const std::string& name,
    DialAppState app_state) {
  auto app_info = std::make_unique<ParsedDialAppInfo>();
  app_info->name = name;
  app_info->state = app_state;
  return app_info;
}

}  // namespace

class TestSafeDialAppInfoParser : public SafeDialAppInfoParser {
 public:
  TestSafeDialAppInfoParser() : SafeDialAppInfoParser(nullptr) {}
  ~TestSafeDialAppInfoParser() override {}

  MOCK_METHOD1(ParseInternal, void(const std::string& xml_text));

  void Parse(const std::string& xml_text, ParseCallback callback) override {
    parse_callback_ = std::move(callback);
    ParseInternal(xml_text);
  }

  void InvokeParseCallback(std::unique_ptr<ParsedDialAppInfo> app_info,
                           ParsingResult parsing_result) {
    if (!parse_callback_)
      return;
    std::move(parse_callback_).Run(std::move(app_info), parsing_result);
  }

 private:
  ParseCallback parse_callback_;
};

class DialAppDiscoveryServiceTest : public ::testing::Test {
 public:
  DialAppDiscoveryServiceTest()
      : test_parser_(new TestSafeDialAppInfoParser()),
        dial_app_discovery_service_(nullptr, mock_completed_cb_.Get()) {
    dial_app_discovery_service_.SetParserForTest(
        std::unique_ptr<TestSafeDialAppInfoParser>(test_parser_));
  }

  void TearDown() override {
    dial_app_discovery_service_.pending_fetcher_map_.clear();
  }

 protected:
  base::MockCallback<DialAppDiscoveryService::DialAppInfoParseCompletedCallback>
      mock_completed_cb_;

  TestSafeDialAppInfoParser* test_parser_;
  DialAppDiscoveryService dial_app_discovery_service_;
  const content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  const net::TestURLFetcherFactory factory_;
};

TEST_F(DialAppDiscoveryServiceTest, TestFetchDialAppInfoFetchURL) {
  MediaSinkInternal dial_sink = CreateDialSink(1);
  EXPECT_CALL(mock_completed_cb_, Run(dial_sink.sink().id(), kYouTubeName,
                                      SinkAppStatus::kAvailable));
  dial_app_discovery_service_.FetchDialAppInfo(dial_sink, kYouTubeName,
                                               profile_.GetRequestContext());

  EXPECT_CALL(*test_parser_, ParseInternal(_))
      .WillOnce(Invoke([&](const std::string& xml_text) {
        test_parser_->InvokeParseCallback(
            CreateParsedDialAppInfo(kYouTubeName, DialAppState::kRunning),
            SafeDialAppInfoParser::ParsingResult::kSuccess);
      }));
  net::TestURLFetcher* test_fetcher =
      factory_.GetFetcherByID(DialAppInfoFetcher::kURLFetcherIDForTest);
  GURL expected_url("http://192.168.0.101/apps/YouTube");
  EXPECT_EQ(expected_url, test_fetcher->GetOriginalURL());
  test_fetcher->set_response_code(net::HTTP_OK);
  test_fetcher->SetResponseString("<xml>appInfo</xml>");
  test_fetcher->delegate()->OnURLFetchComplete(test_fetcher);
}

TEST_F(DialAppDiscoveryServiceTest,
       TestFetchDialAppInfoFetchURLTransientError) {
  MediaSinkInternal dial_sink = CreateDialSink(1);
  EXPECT_CALL(mock_completed_cb_, Run(_, _, _)).Times(0);
  dial_app_discovery_service_.FetchDialAppInfo(dial_sink, kYouTubeName,
                                               profile_.GetRequestContext());

  net::TestURLFetcher* test_fetcher =
      factory_.GetFetcherByID(DialAppInfoFetcher::kURLFetcherIDForTest);
  test_fetcher->set_response_code(net::HTTP_TEMPORARY_REDIRECT);
  test_fetcher->delegate()->OnURLFetchComplete(test_fetcher);
}

TEST_F(DialAppDiscoveryServiceTest, TestFetchDialAppInfoFetchURLError) {
  MediaSinkInternal dial_sink = CreateDialSink(1);
  EXPECT_CALL(mock_completed_cb_, Run(dial_sink.sink().id(), kYouTubeName,
                                      SinkAppStatus::kUnavailable));
  dial_app_discovery_service_.FetchDialAppInfo(dial_sink, kYouTubeName,
                                               profile_.GetRequestContext());

  net::TestURLFetcher* test_fetcher =
      factory_.GetFetcherByID(DialAppInfoFetcher::kURLFetcherIDForTest);
  test_fetcher->set_response_code(net::HTTP_NOT_FOUND);
  test_fetcher->delegate()->OnURLFetchComplete(test_fetcher);

  EXPECT_CALL(mock_completed_cb_, Run(_, _, SinkAppStatus::kUnavailable))
      .Times(0);
  dial_app_discovery_service_.FetchDialAppInfo(dial_sink, kYouTubeName,
                                               profile_.GetRequestContext());
}

TEST_F(DialAppDiscoveryServiceTest, TestFetchDialAppInfoParseError) {
  MediaSinkInternal dial_sink = CreateDialSink(1);
  EXPECT_CALL(mock_completed_cb_, Run(dial_sink.sink().id(), kYouTubeName,
                                      SinkAppStatus::kUnavailable));
  dial_app_discovery_service_.FetchDialAppInfo(dial_sink, kYouTubeName,
                                               profile_.GetRequestContext());

  EXPECT_CALL(*test_parser_, ParseInternal(_))
      .WillOnce(Invoke([&](const std::string& xml_text) {
        test_parser_->InvokeParseCallback(
            nullptr, SafeDialAppInfoParser::ParsingResult::kMissingName);
      }));
  net::TestURLFetcher* test_fetcher =
      factory_.GetFetcherByID(DialAppInfoFetcher::kURLFetcherIDForTest);
  test_fetcher->set_response_code(net::HTTP_OK);
  test_fetcher->SetResponseString("<xml>appInfo</xml>");
  test_fetcher->delegate()->OnURLFetchComplete(test_fetcher);
}

TEST_F(DialAppDiscoveryServiceTest, TestGetAvailabilityFromAppInfoAvailable) {
  MediaSinkInternal dial_sink = CreateDialSink(1);
  auto parsed_app_info =
      CreateParsedDialAppInfo(kNetflixName, DialAppState::kRunning);

  EXPECT_CALL(mock_completed_cb_, Run(dial_sink.sink().id(), kNetflixName,
                                      SinkAppStatus::kAvailable));
  dial_app_discovery_service_.OnDialAppInfoParsed(
      dial_sink.sink().id(), kNetflixName, std::move(parsed_app_info),
      SafeDialAppInfoParser::ParsingResult::kSuccess);
}

TEST_F(DialAppDiscoveryServiceTest, TestGetAvailabilityFromAppInfoUnavailable) {
  MediaSinkInternal dial_sink = CreateDialSink(1);
  auto parsed_app_info =
      CreateParsedDialAppInfo(kNetflixName, DialAppState::kUnknown);

  EXPECT_CALL(mock_completed_cb_, Run(dial_sink.sink().id(), kNetflixName,
                                      SinkAppStatus::kUnavailable));
  dial_app_discovery_service_.OnDialAppInfoParsed(
      dial_sink.sink().id(), kNetflixName, std::move(parsed_app_info),
      SafeDialAppInfoParser::ParsingResult::kSuccess);
}

}  // namespace media_router
