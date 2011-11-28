// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "content/browser/speech/speech_recognition_request.h"
#include "content/public/common/speech_input_result.h"
#include "content/test/test_url_fetcher_factory.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace speech_input {

class SpeechRecognitionRequestTest : public SpeechRecognitionRequestDelegate,
                                     public testing::Test {
 public:
  SpeechRecognitionRequestTest() { }

  // Creates a speech recognition request and invokes it's URL fetcher delegate
  // with the given test data.
  void CreateAndTestRequest(bool success, const std::string& http_response);

  // SpeechRecognitionRequestDelegate methods.
  virtual void SetRecognitionResult(
      const content::SpeechInputResult& result) OVERRIDE {
    result_ = result;
  }

 protected:
  MessageLoop message_loop_;
  TestURLFetcherFactory url_fetcher_factory_;
  content::SpeechInputResult result_;
};

void SpeechRecognitionRequestTest::CreateAndTestRequest(
    bool success, const std::string& http_response) {
  SpeechRecognitionRequest request(NULL, this);
  request.Start(std::string(), std::string(), false, std::string(),
                std::string(), std::string());
  request.UploadAudioChunk(std::string(" "), true);
  TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);

  fetcher->set_url(fetcher->GetOriginalURL());
  net::URLRequestStatus status;
  status.set_status(success ? net::URLRequestStatus::SUCCESS :
                              net::URLRequestStatus::FAILED);
  fetcher->set_status(status);
  fetcher->set_response_code(success ? 200 : 500);
  fetcher->SetResponseString(http_response);

  fetcher->delegate()->OnURLFetchComplete(fetcher);
  // Parsed response will be available in result_.
}

TEST_F(SpeechRecognitionRequestTest, BasicTest) {
  // Normal success case with one result.
  CreateAndTestRequest(true,
      "{\"status\":0,\"hypotheses\":"
      "[{\"utterance\":\"123456\",\"confidence\":0.9}]}");
  EXPECT_EQ(result_.error, content::SPEECH_INPUT_ERROR_NONE);
  EXPECT_EQ(1U, result_.hypotheses.size());
  EXPECT_EQ(ASCIIToUTF16("123456"), result_.hypotheses[0].utterance);
  EXPECT_EQ(0.9, result_.hypotheses[0].confidence);

  // Normal success case with multiple results.
  CreateAndTestRequest(true,
      "{\"status\":0,\"hypotheses\":["
      "{\"utterance\":\"hello\",\"confidence\":0.9},"
      "{\"utterance\":\"123456\",\"confidence\":0.5}]}");
  EXPECT_EQ(result_.error, content::SPEECH_INPUT_ERROR_NONE);
  EXPECT_EQ(2u, result_.hypotheses.size());
  EXPECT_EQ(ASCIIToUTF16("hello"), result_.hypotheses[0].utterance);
  EXPECT_EQ(0.9, result_.hypotheses[0].confidence);
  EXPECT_EQ(ASCIIToUTF16("123456"), result_.hypotheses[1].utterance);
  EXPECT_EQ(0.5, result_.hypotheses[1].confidence);

  // Zero results.
  CreateAndTestRequest(true, "{\"status\":0,\"hypotheses\":[]}");
  EXPECT_EQ(result_.error, content::SPEECH_INPUT_ERROR_NONE);
  EXPECT_EQ(0U, result_.hypotheses.size());

  // Http failure case.
  CreateAndTestRequest(false, "");
  EXPECT_EQ(result_.error, content::SPEECH_INPUT_ERROR_NETWORK);
  EXPECT_EQ(0U, result_.hypotheses.size());

  // Invalid status case.
  CreateAndTestRequest(true, "{\"status\":\"invalid\",\"hypotheses\":[]}");
  EXPECT_EQ(result_.error, content::SPEECH_INPUT_ERROR_NETWORK);
  EXPECT_EQ(0U, result_.hypotheses.size());

  // Server-side error case.
  CreateAndTestRequest(true, "{\"status\":1,\"hypotheses\":[]}");
  EXPECT_EQ(result_.error, content::SPEECH_INPUT_ERROR_NETWORK);
  EXPECT_EQ(0U, result_.hypotheses.size());

  // Malformed JSON case.
  CreateAndTestRequest(true, "{\"status\":0,\"hypotheses\":"
      "[{\"unknownkey\":\"hello\"}]}");
  EXPECT_EQ(result_.error, content::SPEECH_INPUT_ERROR_NETWORK);
  EXPECT_EQ(0U, result_.hypotheses.size());
}

}  // namespace speech_input
