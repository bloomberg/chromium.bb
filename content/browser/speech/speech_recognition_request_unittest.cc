// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "chrome/common/net/test_url_fetcher_factory.h"
#include "content/browser/speech/speech_recognition_request.h"
#include "net/url_request/url_request_status.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace speech_input {

class SpeechRecognitionRequestTest : public SpeechRecognitionRequestDelegate,
                                     public testing::Test {
 public:
  SpeechRecognitionRequestTest() : error_(false) { }

  // Creates a speech recognition request and invokes it's URL fetcher delegate
  // with the given test data.
  void CreateAndTestRequest(bool success, const std::string& http_response);

  // SpeechRecognitionRequestDelegate methods.
  virtual void SetRecognitionResult(bool error,
                                    const SpeechInputResultArray& result) {
    error_ = error;
    result_ = result;
  }

  // testing::Test methods.
  virtual void SetUp() {
    URLFetcher::set_factory(&url_fetcher_factory_);
  }

  virtual void TearDown() {
    URLFetcher::set_factory(NULL);
  }

 protected:
  MessageLoop message_loop_;
  TestURLFetcherFactory url_fetcher_factory_;
  bool error_;
  SpeechInputResultArray result_;
};

void SpeechRecognitionRequestTest::CreateAndTestRequest(
    bool success, const std::string& http_response) {
  SpeechRecognitionRequest request(NULL, this);
  request.Send(std::string(), std::string(), std::string(), std::string(),
               std::string(), std::string());
  TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  net::URLRequestStatus status;
  status.set_status(success ? net::URLRequestStatus::SUCCESS :
                              net::URLRequestStatus::FAILED);
  fetcher->delegate()->OnURLFetchComplete(fetcher, fetcher->original_url(),
                                          status, success ? 200 : 500,
                                          ResponseCookies(),
                                          http_response);
  // Parsed response will be available in result_.
}

TEST_F(SpeechRecognitionRequestTest, BasicTest) {
  // Normal success case with one result.
  CreateAndTestRequest(true,
      "{\"hypotheses\":[{\"utterance\":\"123456\",\"confidence\":0.9}]}");
  EXPECT_FALSE(error_);
  EXPECT_EQ(1U, result_.size());
  EXPECT_EQ(ASCIIToUTF16("123456"), result_[0].utterance);
  EXPECT_EQ(0.9, result_[0].confidence);

  // Normal success case with multiple results.
  CreateAndTestRequest(true,
      "{\"hypotheses\":[{\"utterance\":\"hello\",\"confidence\":0.9},"
      "{\"utterance\":\"123456\",\"confidence\":0.5}]}");
  EXPECT_FALSE(error_);
  EXPECT_EQ(2u, result_.size());
  EXPECT_EQ(ASCIIToUTF16("hello"), result_[0].utterance);
  EXPECT_EQ(0.9, result_[0].confidence);
  EXPECT_EQ(ASCIIToUTF16("123456"), result_[1].utterance);
  EXPECT_EQ(0.5, result_[1].confidence);

  // Http failure case.
  CreateAndTestRequest(false, "");
  EXPECT_TRUE(error_);
  EXPECT_EQ(0U, result_.size());

  // Malformed JSON case.
  CreateAndTestRequest(true, "{\"hypotheses\":[{\"unknownkey\":\"hello\"}]}");
  EXPECT_TRUE(error_);
  EXPECT_EQ(0U, result_.size());
}

}  // namespace speech_input
