// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/intents/cws_intents_registry.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/extensions/extension_l10n_util.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

// Create a CWSIntentsRegistry proxy for testing purposes.
// Needs to be non-anonymous so it can be friended.
class CWSIntentsRegistryForTest {
public:
  CWSIntentsRegistryForTest() : registry_(NULL) {}
  ~CWSIntentsRegistryForTest() { delete registry_; }

  void reset(net::URLRequestContextGetter* context) {
    delete registry_;
    registry_ = new CWSIntentsRegistry(context);
  }

  CWSIntentsRegistry*  operator->() { return registry_; }

private:
  CWSIntentsRegistry* registry_;
};

namespace {

const char kCWSResponseInvalid[] =
    "{\"error\":{"
      "\"errors\":[{"
        "\"domain\":\"global\","
        "\"reason\":\"invalid\","
        "\"message\":\"Invalid mimetype:foo\"}],"
      "\"code\":400,"
      "\"message\":\"Invalid mimetype:foo\"}}\"";

const char kCWSResponseValid[] =
  "{\"kind\":\"chromewebstore#itemList\","
  " \"total_items\":1,"
  " \"start_index\":0,"
  " \"items\":[  "
  "  {\"kind\":\"chromewebstore#item\","
  "   \"id\":\"nhkckhebbbncbkefhcpcgepcgfaclehe\","
  "   \"type\":\"APPLICATION\","
  "   \"num_ratings\":0,"
  "   \"average_rating\":0.0,"
  "   \"manifest\":\"{\\n\\\"update_url\\\":\\"
  "\"http://0.tbhome_staging.dserver.download-qa.td.borg.google.com/"
  "service/update2/crx\\\",\\n  \\\"name\\\": \\\"Sidd's Intent App\\\""
  ",\\n  \\\"description\\\": \\\"Do stuff\\\",\\n  \\\"version\\\": "
  "\\\"1.2.19\\\",\\n  \\\"app\\\": {\\n    \\\"urls\\\": [     \\n    ],"
  "\\n    \\\"launch\\\": {\\n      \\\"web_url\\\": \\"
  "\"http://siddharthasaha.net/\\\"\\n    }\\n  },\\n  \\\"icons\\\": "
  "{\\n    \\\"128\\\": \\\"icon128.png\\\"\\n  },\\n  \\\"permissions\\\":"
  " [\\n    \\\"unlimitedStorage\\\",\\n    \\\"notifications\\\"\\n  ],\\n"
  "  \\\"intents\\\": {\\n    \\\"http://webintents.org/edit\\\" : {\\n      "
  "\\\"type\\\" : [\\\"image/png\\\", \\\"image/jpg\\\"],\\n      \\\"path\\"
  "\" : \\\"//services/edit\\\",\\n      \\\"title\\\" : "
  "\\\"Sample Editing Intent\\\",\\n      \\\"disposition\\\" : \\\"inline\\"
  "\"\\n    },\\n    \\\"http://webintents.org/share\\\" : "
  "{\\n      \\\"type\\\" : [\\\"text/plain\\\", \\\"image/jpg\\\"],"
  "\\n      \\\"path\\\" : \\\"//services/share\\\",\\n      \\\"title\\\" : "
  "\\\"Sample sharing Intent\\\",\\n      \\\"disposition\\\" : "
  "\\\"inline\\\"\\n    }\\n  }\\n}\\n\","
  "   \"family_safe\":true,"
  "   \"icon_url\":\"http://qa-lighthouse.sandbox.google.com/image/"
  "QzPnRCYCBbBGI99ZkGxkp-NNJ488IkkiTyCgynFEeDTJHcw4tHl3csmjTQ\"}]}";

const char kCWSResponseValidL10n[] =
  "{\"kind\":\"chromewebstore#itemList\","
  " \"total_items\":1,"
  " \"start_index\":0,"
  " \"items\":[  "
  "  {\"kind\":\"chromewebstore#item\","
  "   \"id\":\"nhkckhebbbncbkefhcpcgepcgfaclehe\","
  "   \"type\":\"APPLICATION\","
  "   \"num_ratings\":0,"
  "   \"average_rating\":0.0,"
  "   \"manifest\":\"{\\n\\\"update_url\\\":\\"
  "\"http://0.tbhome_staging.dserver.download-qa.td.borg.google.com/"
  "service/update2/crx\\\",\\n  \\\"name\\\": \\\"__MSG_name__\\\""
  ",\\n  \\\"description\\\": \\\"Do stuff\\\",\\n  \\\"version\\\": "
  "\\\"1.2.19\\\",\\n  \\\"app\\\": {\\n    \\\"urls\\\": [     \\n    ],"
  "\\n    \\\"launch\\\": {\\n      \\\"web_url\\\": \\"
  "\"http://siddharthasaha.net/\\\"\\n    }\\n  },\\n  \\\"icons\\\": "
  "{\\n    \\\"128\\\": \\\"icon128.png\\\"\\n  },\\n  \\\"permissions\\\":"
  " [\\n    \\\"unlimitedStorage\\\",\\n    \\\"notifications\\\"\\n  ],\\n"
  "  \\\"intents\\\": {\\n    \\\"http://webintents.org/edit\\\" : {\\n      "
  "\\\"type\\\" : [\\\"image/png\\\", \\\"image/jpg\\\"],\\n      \\\"path\\"
  "\" : \\\"//services/edit\\\",\\n      \\\"title\\\" : "
  "\\\"Sample Editing Intent\\\",\\n      \\\"disposition\\\" : \\\"inline\\"
  "\"\\n    },\\n    \\\"http://webintents.org/share\\\" : "
  "{\\n      \\\"type\\\" : [\\\"text/plain\\\", \\\"image/jpg\\\"],"
  "\\n      \\\"path\\\" : \\\"//services/share\\\",\\n      \\\"title\\\" : "
  "\\\"Sample sharing Intent\\\",\\n      \\\"disposition\\\" : "
  "\\\"inline\\\"\\n    }\\n  }\\n}\\n\","
  "   \"family_safe\":true,"
  "   \"icon_url\":\"http://qa-lighthouse.sandbox.google.com/image/"
  "QzPnRCYCBbBGI99ZkGxkp-NNJ488IkkiTyCgynFEeDTJHcw4tHl3csmjTQ\","
  "\"locale_data\": [";

const char kCWSResponseValidL10nPostfix[] =
"]}]}";

const char kLocaleDataEn[] =
  "{"
  "  \"locale_string\": \"en\","
  "  \"title\": \"Localized EN\""
  "}";

const char kLocaleDataDe[] =
  "{"
  "  \"locale_string\": \"de\","
  "  \"title\": \"Localized DE\""
  "}";

const char kLocaleDataAll[] =
  "{"
  "  \"locale_string\": \"all\","
  "  \"title\": \"Localized ALL\""
  "}";

const char kValidIconURL[] =
    "http://qa-lighthouse.sandbox.google.com/image/"
    "QzPnRCYCBbBGI99ZkGxkp-NNJ488IkkiTyCgynFEeDTJHcw4tHl3csmjTQ";

const char kValidManifest[] =
    "{\n\"update_url\":\"http://0.tbhome_staging.dserver.download-qa.td.borg."
    "google.com/service/update2/crx\",\n  \"name\": \"Sidd's Intent App\",\n"
    "  \"description\": \"Do stuff\",\n  \"version\": \"1.2.19\",\n  \"app\":"
    " {\n    \"urls\": [     \n    ],\n    \"launch\": {\n      \"web_url\":"
    " \"http://siddharthasaha.net/\"\n    }\n  },\n  \"icons\": {\n    "
    "\"128\": \"icon128.png\"\n  },\n  \"permissions\": [\n    "
    "\"unlimitedStorage\",\n    \"notifications\"\n  ],\n  \"intents\": "
    "{\n    \"http://webintents.org/edit\" : {\n      \"type\" : ["
    "\"image/png\", \"image/jpg\"],\n      \"path\" : \"//services/edit\",\n"
    "      \"title\" : \"Sample Editing Intent\",\n      \"disposition\" : "
    "\"inline\"\n    },\n    \"http://webintents.org/share\" : {\n      "
    "\"type\" : [\"text/plain\", \"image/jpg\"],\n      \"path\" : "
    "\"//services/share\",\n      \"title\" : \"Sample sharing Intent\",\n"
    "      \"disposition\" : \"inline\"\n    }\n  }\n}\n";

const char kEditAction[] = "http://webintents.org/edit";
const char kImagePngType[] = "image/png";

class ScopedLocale {
 public:
  ScopedLocale() : locale_(extension_l10n_util::CurrentLocaleOrDefault()) {}
  ~ScopedLocale() { extension_l10n_util::SetProcessLocale(locale_); }
  std::string locale_;
};

class CWSIntentsRegistryTest : public testing::Test {
 public:
  virtual void SetUp() {
    scoped_refptr<TestURLRequestContextGetter> context_getter_(
        new TestURLRequestContextGetter(ui_loop_.message_loop_proxy()));
    registry_.reset(context_getter_);
  }

  virtual void TearDown() {
    // Pump messages posted by the main thread.
    ui_loop_.RunAllPending();
  }

  CWSIntentsRegistry::IntentExtensionList WaitForResults() {
    ui_loop_.RunAllPending();
    return extensions_;
  }

  void Callback(const CWSIntentsRegistry::IntentExtensionList& extensions) {
    extensions_ = extensions;
  }

  void RunRequest(const std::string& action, const std::string& mime,
      const std::string& response) {
    extensions_.clear();

    net::FakeURLFetcherFactory test_factory;
    test_factory.SetFakeResponse(
        CWSIntentsRegistry::BuildQueryURL(
            ASCIIToUTF16(action),ASCIIToUTF16(mime)).spec(),
        response, true);

    registry_->GetIntentServices(ASCIIToUTF16(action),
                                 ASCIIToUTF16(mime),
                                 base::Bind(&CWSIntentsRegistryTest::Callback,
                                            base::Unretained(this)));
    WaitForResults();
  }


 protected:
  CWSIntentsRegistryForTest registry_;
  CWSIntentsRegistry::IntentExtensionList extensions_;
  MessageLoop ui_loop_;
};

}  // namespace

TEST_F(CWSIntentsRegistryTest, ValidQuery) {
  RunRequest(kEditAction, kImagePngType,kCWSResponseValid);
  ASSERT_EQ(1UL, extensions_.size());

  EXPECT_EQ(0, extensions_[0].num_ratings);
  EXPECT_EQ(0.0, extensions_[0].average_rating);
  EXPECT_EQ(std::string(kValidManifest), UTF16ToUTF8(extensions_[0].manifest));
  EXPECT_EQ(std::string("nhkckhebbbncbkefhcpcgepcgfaclehe"),
            UTF16ToUTF8(extensions_[0].id) );
  EXPECT_EQ(std::string("Sidd's Intent App"),
            UTF16ToUTF8(extensions_[0].name));
  EXPECT_EQ(std::string(kValidIconURL), extensions_[0].icon_url.spec());
}

TEST_F(CWSIntentsRegistryTest, InvalidQuery) {
  RunRequest("foo", "foo",kCWSResponseInvalid);
  EXPECT_EQ(0UL, extensions_.size());
}

TEST_F(CWSIntentsRegistryTest, BuildQueryURL) {
  const std::string kExpectedURL = "https://www.googleapis.com"
      "/chromewebstore/v1.1b/items/intent"
      "?intent=action&mime_types=mime%2Ftype&start_index=0&num_results=15";
  GURL url = CWSIntentsRegistry::BuildQueryURL(ASCIIToUTF16("action"),
                                               ASCIIToUTF16("mime/type"));

  EXPECT_EQ(kExpectedURL, url.spec().substr(0, kExpectedURL.size()));
}

// Test for match to the application locale - i.e. if running in "en",
// registry will use locale_data for "en" key, with or without "all"
// locale_data present.
TEST_F(CWSIntentsRegistryTest, LocalizeMatchingLocale) {
  ScopedLocale restoreLocaleOnExit;

  std::string response = kCWSResponseValidL10n;
  response += kLocaleDataEn + std::string(",");
  response += kLocaleDataAll + std::string(",");
  response += kLocaleDataDe;
  response += kCWSResponseValidL10nPostfix;

  // Picks the proper locale_data based on application locale.
  extension_l10n_util::SetProcessLocale("en");
  RunRequest(kEditAction, kImagePngType, response);
  ASSERT_EQ(1UL, extensions_.size());
  EXPECT_EQ(std::string("Localized EN"),
            UTF16ToUTF8(extensions_[0].name));

  extension_l10n_util::SetProcessLocale("de");
  RunRequest(kEditAction, kImagePngType, response);
  ASSERT_EQ(1UL, extensions_.size());
  EXPECT_EQ(std::string("Localized DE"),
            UTF16ToUTF8(extensions_[0].name));

  // Falls back to locale_data for "All" if unknown application locale.
  extension_l10n_util::SetProcessLocale("fr");
  RunRequest(kEditAction, kImagePngType, std::string(kCWSResponseValidL10n) +
      kLocaleDataAll + kCWSResponseValidL10nPostfix);
  ASSERT_EQ(1UL, extensions_.size());
  EXPECT_EQ(std::string("Localized ALL"),
            UTF16ToUTF8(extensions_[0].name));

  // Keeps original content if unknown application locale and no "all"
  // localization data exists.
  response = kCWSResponseValidL10n;
  response += kLocaleDataEn;
  response += kCWSResponseValidL10nPostfix;
  extension_l10n_util::SetProcessLocale("de");
  RunRequest(kEditAction, kImagePngType, response);
  ASSERT_EQ(1UL, extensions_.size());
  EXPECT_EQ(std::string("__MSG_name__"),
            UTF16ToUTF8(extensions_[0].name));
}
