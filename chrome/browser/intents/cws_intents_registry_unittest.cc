// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/intents/cws_intents_registry.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "content/test/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kCWSQueryInvalid[] =
    "https://www-googleapis-staging.sandbox.google.com"
    "/chromewebstore/v1.1b/items/intent"
      "?intent=foo&mime_types=foo";
const char kCWSResponseInvalid[] =
    "{\"error\":{\"errors\":[{\"domain\":\"global\",\"reason\":\"invalid\","
    "\"message\":\"Invalid mimetype:foo\"}],\"code\":400,"
    "\"message\":\"Invalid mimetype:foo\"}}\"";

const char kCWSQueryValid[] =
    "https://www-googleapis-staging.sandbox.google.com"
    "/chromewebstore/v1.1b/items/intent"
      "?intent=http%3A%2F%2Fwebintents.org%2Fedit&mime_types=*%2Fpng";
const char kCWSResponseValid[] =
  "{\"kind\":\"chromewebstore#itemList\",\"total_items\":1,"
  "\"start_index\":0,\"items\":[{\"kind\":\"chromewebstore#item\","
  "\"id\":\"nhkckhebbbncbkefhcpcgepcgfaclehe\",\"type\":\"APPLICATION\","
  "\"num_ratings\":0,\"average_rating\":0.0,\"manifest\":"
  "\"{\\n\\\"update_url\\\":\\"
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
  "\\\"inline\\\"\\n    }\\n  }\\n}\\n\",\"family_safe\":true,\"icon_url\":"
  "\"http://qa-lighthouse.sandbox.google.com/image/"
  "QzPnRCYCBbBGI99ZkGxkp-NNJ488IkkiTyCgynFEeDTJHcw4tHl3csmjTQ\"}]}";
const char kValidIconURL[]=
    "http://qa-lighthouse.sandbox.google.com/image/"
    "QzPnRCYCBbBGI99ZkGxkp-NNJ488IkkiTyCgynFEeDTJHcw4tHl3csmjTQ";
const char kValidManifest[]=
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

class CWSIntentsRegistryTest : public testing::Test {
 public:
  CWSIntentsRegistryTest() : test_factory_(NULL) {}
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

  CWSIntentsRegistry::IntentExtensionList extensions_;
  FakeURLFetcherFactory test_factory_;

 protected:
  MessageLoop ui_loop_;
};

}  // namespace

TEST_F(CWSIntentsRegistryTest, ValidQuery) {
  const scoped_refptr<TestURLRequestContextGetter> context_getter(
      new TestURLRequestContextGetter(ui_loop_.message_loop_proxy()));
  test_factory_.SetFakeResponse(kCWSQueryValid, kCWSResponseValid, true);

  CWSIntentsRegistry registry(context_getter);
  registry.GetIntentServices(ASCIIToUTF16("http://webintents.org/edit"),
                             ASCIIToUTF16("*/png"),
                             base::Bind(&CWSIntentsRegistryTest::Callback,
                                        base::Unretained(this)));

  WaitForResults();
  ASSERT_EQ(1UL, extensions_.size());

  EXPECT_EQ(0, extensions_[0].num_ratings);
  EXPECT_EQ(0.0, extensions_[0].average_rating);
  EXPECT_EQ(std::string(kValidManifest), UTF16ToUTF8(extensions_[0].manifest));
  EXPECT_EQ(std::string(kValidIconURL), extensions_[0].icon_url.spec());
}

TEST_F(CWSIntentsRegistryTest, InvalidQuery) {
  const scoped_refptr<TestURLRequestContextGetter> context_getter(
      new TestURLRequestContextGetter(ui_loop_.message_loop_proxy()));
  test_factory_.SetFakeResponse(kCWSQueryInvalid, kCWSResponseInvalid, true);

  CWSIntentsRegistry registry(context_getter);
  registry.GetIntentServices(ASCIIToUTF16("foo"),
                             ASCIIToUTF16("foo"),
                             base::Bind(&CWSIntentsRegistryTest::Callback,
                                        base::Unretained(this)));

  WaitForResults();
  EXPECT_EQ(0UL, extensions_.size());
}
