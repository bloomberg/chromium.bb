// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/spellchecker/spelling_service_client.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/spellcheck_result.h"
#include "chrome/test/base/testing_profile.h"
#include "net/base/load_flags.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// A mock URL fetcher used in the TestingSpellingServiceClient class. This class
// verifies JSON-RPC requests when the SpellingServiceClient class sends them to
// the Spelling service. This class also verifies the SpellingServiceClient
// class does not either send cookies to the Spelling service or accept cookies
// from it.
class TestSpellingURLFetcher : public net::TestURLFetcher {
 public:
  TestSpellingURLFetcher(int id,
                         const GURL& url,
                         net::URLFetcherDelegate* d,
                         int version,
                         const std::string& text,
                         const std::string& language,
                         int status,
                         const std::string& response)
      : net::TestURLFetcher(0, url, d),
        version_(base::StringPrintf("v%d", version)),
        language_(language.empty() ? std::string("en") : language),
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
    EXPECT_EQ(language_, language);
    ASSERT_TRUE(GetExpectedCountry(language, &country_));
    std::string country;
    EXPECT_TRUE(value->GetString("params.originCountry", &country));
    EXPECT_EQ(country_, country);

    net::TestURLFetcher::SetUploadData(upload_content_type, upload_content);
  }

  virtual void Start() OVERRIDE {
    // Verify that this client does not either send cookies to the Spelling
    // service or accept cookies from it.
    EXPECT_EQ(net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SAVE_COOKIES,
              GetLoadFlags());
  }

 private:
  bool GetExpectedCountry(const std::string& language, std::string* country) {
    static const struct {
      const char* language;
      const char* country;
    } kCountries[] = {
      {"af", "ZAF"},
      {"en", "USA"},
    };
    for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kCountries); ++i) {
      if (!language.compare(kCountries[i].language)) {
        country->assign(kCountries[i].country);
        return true;
      }
    }
    return false;
  }

  std::string version_;
  std::string language_;
  std::string country_;
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

  void SetHTTPRequest(int type,
                      const std::string& text,
                      const std::string& language) {
    request_type_ = type;
    request_text_ = text;
    request_language_ = language;
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
                      const string16& request_text,
                      const std::vector<SpellCheckResult>& results) {
    EXPECT_EQ(success_, success);
    string16 text(UTF8ToUTF16(request_text_));
    EXPECT_EQ(text, request_text);
    for (std::vector<SpellCheckResult>::const_iterator it = results.begin();
         it != results.end(); ++it) {
      text.replace(it->location, it->length, it->replacement);
    }
    EXPECT_EQ(corrected_text_, text);
  }

 private:
  virtual net::URLFetcher* CreateURLFetcher(const GURL& url) OVERRIDE {
    EXPECT_EQ("https://www.googleapis.com/rpc", url.spec());
    fetcher_ = new TestSpellingURLFetcher(0, url, this,
                                          request_type_, request_text_,
                                          request_language_,
                                          response_status_, response_data_);
    return fetcher_;
  }

  int request_type_;
  std::string request_text_;
  std::string request_language_;
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

  virtual void SetUp() OVERRIDE {
  }

  void OnTextCheckComplete(int tag,
                           bool success,
                           const string16& text,
                           const std::vector<SpellCheckResult>& results) {
    client_.VerifyResponse(success, text, results);
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
    const char* language;
  } kTests[] = {
    {
      "",
      SpellingServiceClient::SUGGEST,
      500,
      "",
      false,
      "",
      "af",
    }, {
      "chromebook",
      SpellingServiceClient::SUGGEST,
      200,
      "{}",
      true,
      "chromebook",
      "af",
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
      "af",
    }, {
      "",
      SpellingServiceClient::SPELLCHECK,
      500,
      "",
      false,
      "",
      "en",
    }, {
      "I have been to USA.",
      SpellingServiceClient::SPELLCHECK,
      200,
      "{}",
      true,
      "I have been to USA.",
      "en",
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
      "en",
    },
  };

  PrefService* pref = profile_.GetPrefs();
  pref->SetBoolean(prefs::kEnableContinuousSpellcheck, true);
  pref->SetBoolean(prefs::kSpellCheckUseSpellingService, true);

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTests); ++i) {
    client_.SetHTTPRequest(kTests[i].request_type, kTests[i].request_text,
                           kTests[i].language);
    client_.SetHTTPResponse(kTests[i].response_status, kTests[i].response_data);
    client_.SetExpectedTextCheckResult(kTests[i].success,
                                       kTests[i].corrected_text);
    pref->SetString(prefs::kSpellCheckDictionary, kTests[i].language);
    client_.RequestTextCheck(
        &profile_,
        kTests[i].request_type,
        ASCIIToUTF16(kTests[i].request_text),
        base::Bind(&SpellingServiceClientTest::OnTextCheckComplete,
                   base::Unretained(this), 0));
    client_.CallOnURLFetchComplete();
  }
}

// Verify that SpellingServiceClient::IsAvailable() returns true only when it
// can send suggest requests or spellcheck requests.
TEST_F(SpellingServiceClientTest, AvailableServices) {
  const SpellingServiceClient::ServiceType kSuggest =
      SpellingServiceClient::SUGGEST;
  const SpellingServiceClient::ServiceType kSpellcheck =
      SpellingServiceClient::SPELLCHECK;

  // When a user disables spellchecking or prevent using the Spelling service,
  // this function should return false both for suggestions and for spellcheck.
  PrefService* pref = profile_.GetPrefs();
  pref->SetBoolean(prefs::kEnableContinuousSpellcheck, false);
  pref->SetBoolean(prefs::kSpellCheckUseSpellingService, false);
  EXPECT_FALSE(client_.IsAvailable(&profile_, kSuggest));
  EXPECT_FALSE(client_.IsAvailable(&profile_, kSpellcheck));

  pref->SetBoolean(prefs::kEnableContinuousSpellcheck, true);
  pref->SetBoolean(prefs::kSpellCheckUseSpellingService, true);

  // For locales supported by the SpellCheck service, this function returns
  // false for suggestions and true for spellcheck. (The comment in
  // SpellingServiceClient::IsAvailable() describes why this function returns
  // false for suggestions.) If there is no language set, then we
  // do not allow any remote.
  pref->SetString(prefs::kSpellCheckDictionary, "");
  EXPECT_FALSE(client_.IsAvailable(&profile_, kSuggest));
  EXPECT_FALSE(client_.IsAvailable(&profile_, kSpellcheck));

  static const char* kSupported[] = {
#if !defined(OS_MACOSX)
    "en-AU", "en-CA", "en-GB", "en-US",
#endif
  };
  // If spellcheck is allowed, then suggest is not since spellcheck is a
  // superset of suggest.
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kSupported); ++i) {
    pref->SetString(prefs::kSpellCheckDictionary, kSupported[i]);
    EXPECT_FALSE(client_.IsAvailable(&profile_, kSuggest));
    EXPECT_TRUE(client_.IsAvailable(&profile_, kSpellcheck));
  }

  // This function returns true for suggestions for all and false for
  // spellcheck for unsupported locales.
  static const char* kUnsupported[] = {
#if !defined(OS_MACOSX)
    "af-ZA", "bg-BG", "ca-ES", "cs-CZ", "da-DK", "de-DE", "el-GR", "es-ES",
    "et-EE", "fo-FO", "fr-FR", "he-IL", "hi-IN", "hr-HR", "hu-HU", "id-ID",
    "it-IT", "lt-LT", "lv-LV", "nb-NO", "nl-NL", "pl-PL", "pt-BR", "pt-PT",
    "ro-RO", "ru-RU", "sk-SK", "sl-SI", "sh", "sr", "sv-SE", "tr-TR",
    "uk-UA", "vi-VN",
#endif
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kUnsupported); ++i) {
    pref->SetString(prefs::kSpellCheckDictionary, kUnsupported[i]);
    EXPECT_TRUE(client_.IsAvailable(&profile_, kSuggest));
    EXPECT_FALSE(client_.IsAvailable(&profile_, kSpellcheck));
  }
}
