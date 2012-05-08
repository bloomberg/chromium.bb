// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/json/json_reader.h"
#include "base/memory/scoped_ptr.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/spellchecker/spelling_service_client.h"
#include "chrome/common/spellcheck_result.h"
#include "chrome/test/base/testing_profile.h"
#include "content/test/test_url_fetcher_factory.h"
#include "net/base/load_flags.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// A mock URL fetcher used in the TestingSpellingServiceClient class. This class
// verifies JSON-RPC requests when the SpellingServiceClient class sends them to
// the Spelling service. This class also verifies the SpellingServiceClient
// class does not either send cookies to the Spelling service or accept cookies
// from it.
class TestSpellingURLFetcher : public TestURLFetcher {
 public:
  TestSpellingURLFetcher(int id,
                         const GURL& url,
                         content::URLFetcherDelegate* d,
                         int version,
                         const std::string& text,
                         int status,
                         const std::string& response)
      : TestURLFetcher(0, url, d),
        version_(base::StringPrintf("v%d", version)),
        text_(text) {
    set_response_code(status);
    SetResponseString(response);
  }
  virtual ~TestSpellingURLFetcher() {
  }

  virtual void SetUploadData(const std::string& upload_content_type,
                             const std::string& upload_content) OVERRIDE {
    // Verify the given content type is JSON. (The Spelling service returns an
    // internal server error when this content type is not JSON.)
    EXPECT_EQ("application/json", upload_content_type);

    // Parse the JSON to be sent to the service, and verify its parameters.
    scoped_ptr<DictionaryValue> value(static_cast<DictionaryValue*>(
        base::JSONReader::Read(upload_content,
                               base::JSON_ALLOW_TRAILING_COMMAS)));
    ASSERT_TRUE(!!value.get());
    std::string method;
    EXPECT_TRUE(value->GetString("method", &method));
    EXPECT_EQ("spelling.check", method);
    std::string version;
    EXPECT_TRUE(value->GetString("apiVersion", &version));
    EXPECT_EQ(version_, version);
    std::string text;
    EXPECT_TRUE(value->GetString("params.text", &text));
    EXPECT_EQ(text_, text);
    std::string language;
    EXPECT_TRUE(value->GetString("params.language", &language));
    EXPECT_EQ("en", language);
    std::string country;
    EXPECT_TRUE(value->GetString("params.origin_country", &country));
    EXPECT_EQ("USA", country);

    TestURLFetcher::SetUploadData(upload_content_type, upload_content);
  }

  virtual void Start() OVERRIDE {
    // Verify that this client does not either send cookies to the Spelling
    // service or accept cookies from it.
    EXPECT_EQ(net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SAVE_COOKIES,
              GetLoadFlags());
  }

 private:
  std::string version_;
  std::string text_;
};

// A class derived from the SpellingServiceClient class used by the
// SpellingServiceClientTest class. This class overrides CreateURLFetcher so
// this test can use TestSpellingURLFetcher.
class TestingSpellingServiceClient : public SpellingServiceClient {
 public:
  TestingSpellingServiceClient()
      : request_type_(0),
        response_status_(0),
        success_(false),
        fetcher_(NULL) {
  }
  virtual ~TestingSpellingServiceClient() {
  }

  void SetHTTPRequest(int type, const std::string& text) {
    request_type_ = type;
    request_text_ = text;
  }

  void SetHTTPResponse(int status, const char* data) {
    response_status_ = status;
    response_data_.assign(data);
  }

  void SetExpectedTextCheckResult(bool success, const char* text) {
    success_ = success;
    corrected_text_.assign(UTF8ToUTF16(text));
  }

  void CallOnURLFetchComplete() {
    ASSERT_TRUE(!!fetcher_);
    fetcher_->delegate()->OnURLFetchComplete(fetcher_);
    fetcher_ = NULL;
  }

  void VerifyResponse(bool success,
                      const std::vector<SpellCheckResult>& results) {
    EXPECT_EQ(success_, success);
    string16 text(UTF8ToUTF16(request_text_));
    for (std::vector<SpellCheckResult>::const_iterator it = results.begin();
         it != results.end(); ++it) {
      text.replace(it->location, it->length, it->replacement);
    }
    EXPECT_EQ(corrected_text_, text);
  }

 private:
  virtual content::URLFetcher* CreateURLFetcher(const GURL& url) {
    EXPECT_EQ("https://www.googleapis.com/rpc", url.spec());
    fetcher_ = new TestSpellingURLFetcher(0, url, this,
                                          request_type_, request_text_,
                                          response_status_, response_data_);
    return fetcher_;
  }

  int request_type_;
  std::string request_text_;
  int response_status_;
  std::string response_data_;
  bool success_;
  string16 corrected_text_;
  TestSpellingURLFetcher* fetcher_;  // weak
};

// A test class used for testing the SpellingServiceClient class. This class
// implements a callback function used by the SpellingServiceClient class to
// monitor the class calls the callback with expected results.
class SpellingServiceClientTest : public testing::Test {
 public:
  SpellingServiceClientTest() {}
  virtual ~SpellingServiceClientTest() {}

  void OnTextCheckComplete(int tag,
                           bool success,
                           const std::vector<SpellCheckResult>& results) {
    client_.VerifyResponse(success, results);
  }

 protected:
  TestingSpellingServiceClient client_;
  TestingProfile profile_;
};

}  // namespace

// Verifies that SpellingServiceClient::RequestTextCheck() creates a JSON
// request sent to the Spelling service as we expect. This test also verifies
// that it parses a JSON response from the service and calls the callback
// function. To avoid sending JSON-RPC requests to the service, this test uses a
// custom TestURLFecher class (TestSpellingURLFetcher) which calls
// SpellingServiceClient::OnURLFetchComplete() with the parameters set by this
// test. This test also uses a custom callback function that replaces all
// misspelled words with ones suggested by the service so this test can compare
// the corrected text with the expected results. (If there are not any
// misspelled words, |corrected_text| should be equal to |request_text|.)
TEST_F(SpellingServiceClientTest, RequestTextCheck) {
  static const struct {
    const char* request_text;
    SpellingServiceClient::ServiceType request_type;
    int response_status;
    const char* response_data;
    bool success;
    const char* corrected_text;
  } kTests[] = {
    {
      "",
      SpellingServiceClient::SUGGEST,
      500,
      "",
      false,
      "",
    }, {
      "chromebook",
      SpellingServiceClient::SUGGEST,
      200,
      "{}",
      true,
      "chromebook",
    }, {
      "chrombook",
      SpellingServiceClient::SUGGEST,
      200,
      "{\n"
      "  \"result\": {\n"
      "    \"spellingCheckResponse\": {\n"
      "      \"misspellings\": [{\n"
      "        \"charStart\": 0,\n"
      "        \"charLength\": 9,\n"
      "        \"suggestions\": [{ \"suggestion\": \"chromebook\" }],\n"
      "        \"canAutoCorrect\": false\n"
      "      }]\n"
      "    }\n"
      "  }\n"
      "}",
      true,
      "chromebook",
    }, {
      "",
      SpellingServiceClient::SPELLCHECK,
      500,
      "",
      false,
      "",
    }, {
      "I have been to USA.",
      SpellingServiceClient::SPELLCHECK,
      200,
      "{}",
      true,
      "I have been to USA.",
    }, {
      "I have bean to USA.",
      SpellingServiceClient::SPELLCHECK,
      200,
      "{\n"
      "  \"result\": {\n"
      "    \"spellingCheckResponse\": {\n"
      "      \"misspellings\": [{\n"
      "        \"charStart\": 7,\n"
      "        \"charLength\": 4,\n"
      "        \"suggestions\": [{ \"suggestion\": \"been\" }],\n"
      "        \"canAutoCorrect\": false\n"
      "      }]\n"
      "    }\n"
      "  }\n"
      "}",
      true,
      "I have been to USA.",
    },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTests); ++i) {
    client_.SetHTTPRequest(kTests[i].request_type, kTests[i].request_text);
    client_.SetHTTPResponse(kTests[i].response_status, kTests[i].response_data);
    client_.SetExpectedTextCheckResult(kTests[i].success,
                                       kTests[i].corrected_text);
    client_.RequestTextCheck(
        &profile_,
        0,
        kTests[i].request_type,
        ASCIIToUTF16(kTests[i].request_text),
        base::Bind(&SpellingServiceClientTest::OnTextCheckComplete,
                   base::Unretained(this)));
    client_.CallOnURLFetchComplete();
  }
}
