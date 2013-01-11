// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/manifest_handler.h"
#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"
#include "chrome/common/extensions/web_intents_handler.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/web_intent_service_data.h"

using extensions::Extension;
using extensions::WebIntentsInfo;

namespace errors = extension_manifest_errors;

class WebIntentsManifestTest : public ExtensionManifestTest {
  virtual void SetUp() OVERRIDE {
    ExtensionManifestTest::SetUp();
    extensions::ManifestHandler::Register(extension_manifest_keys::kIntents,
                                          new extensions::WebIntentsHandler);
  }
};

TEST_F(WebIntentsManifestTest, WebIntents) {
  Testcase testcases[] = {
    Testcase("intent_invalid_1.json", errors::kInvalidIntents),
    Testcase("intent_invalid_2.json", errors::kInvalidIntent),
    Testcase("intent_invalid_3.json", errors::kInvalidIntentHref),
    Testcase("intent_invalid_4.json", errors::kInvalidIntentDisposition),
    Testcase("intent_invalid_5.json", errors::kInvalidIntentType),
    Testcase("intent_invalid_6.json", errors::kInvalidIntentTitle),
    Testcase("intent_invalid_packaged_app.json", errors::kCannotAccessPage),
    Testcase("intent_invalid_href_and_path.json",
        errors::kInvalidIntentHrefOldAndNewKey),
    Testcase("intent_invalid_multi_href.json", errors::kInvalidIntent)
  };
  RunTestcases(testcases, arraysize(testcases),
               EXPECT_TYPE_ERROR);

  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("intent_valid.json"));
  ASSERT_TRUE(extension);

  ASSERT_EQ(1u, WebIntentsInfo::GetIntentsServices(extension).size());
  EXPECT_EQ("image/png",
            UTF16ToUTF8(WebIntentsInfo::GetIntentsServices(extension)[0].type));
  EXPECT_EQ("http://webintents.org/share",
      UTF16ToUTF8(WebIntentsInfo::GetIntentsServices(extension)[0].action));
  EXPECT_EQ("chrome-extension",
      WebIntentsInfo::GetIntentsServices(extension)[0].service_url.scheme());
  EXPECT_EQ("/services/share",
      WebIntentsInfo::GetIntentsServices(extension)[0].service_url.path());
  EXPECT_EQ("Sample Sharing Intent",
      UTF16ToUTF8(WebIntentsInfo::GetIntentsServices(extension)[0].title));
  EXPECT_EQ(webkit_glue::WebIntentServiceData::DISPOSITION_INLINE,
            WebIntentsInfo::GetIntentsServices(extension)[0].disposition);

  // Verify that optional fields are filled with defaults.
  extension = LoadAndExpectSuccess("intent_valid_minimal.json");
  ASSERT_TRUE(extension);

  ASSERT_EQ(1u, WebIntentsInfo::GetIntentsServices(extension).size());
  EXPECT_EQ("*",
            UTF16ToUTF8(WebIntentsInfo::GetIntentsServices(extension)[0].type));
  EXPECT_EQ("http://webintents.org/share",
      UTF16ToUTF8(WebIntentsInfo::GetIntentsServices(extension)[0].action));
  EXPECT_EQ("",
      UTF16ToUTF8(WebIntentsInfo::GetIntentsServices(extension)[0].title));
  EXPECT_EQ(webkit_glue::WebIntentServiceData::DISPOSITION_WINDOW,
            WebIntentsInfo::GetIntentsServices(extension)[0].disposition);

  // Make sure we support href instead of path.
  extension = LoadAndExpectSuccess("intent_valid_using_href.json");
  ASSERT_TRUE(extension);

  ASSERT_EQ(1u, WebIntentsInfo::GetIntentsServices(extension).size());
  EXPECT_EQ("/services/share",
      WebIntentsInfo::GetIntentsServices(extension)[0].service_url.path());
}

TEST_F(WebIntentsManifestTest, WebIntentsWithMultipleMimeTypes) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("intent_valid_multitype.json"));
  ASSERT_TRUE(extension);

  ASSERT_EQ(2u, WebIntentsInfo::GetIntentsServices(extension).size());

  // One registration with multiple types generates a separate service for
  // each MIME type.
  for (int i = 0; i < 2; ++i) {
    EXPECT_EQ("http://webintents.org/share",
        UTF16ToUTF8(WebIntentsInfo::GetIntentsServices(extension)[i].action));
    EXPECT_EQ("chrome-extension",
        WebIntentsInfo::GetIntentsServices(extension)[i].service_url.scheme());
    EXPECT_EQ("/services/share",
        WebIntentsInfo::GetIntentsServices(extension)[i].service_url.path());
    EXPECT_EQ("Sample Sharing Intent",
        UTF16ToUTF8(WebIntentsInfo::GetIntentsServices(extension)[i].title));
    EXPECT_EQ(webkit_glue::WebIntentServiceData::DISPOSITION_INLINE,
              WebIntentsInfo::GetIntentsServices(extension)[i].disposition);
  }
  EXPECT_EQ("image/jpeg",
            UTF16ToUTF8(WebIntentsInfo::GetIntentsServices(extension)[0].type));
  EXPECT_EQ("image/bmp",
            UTF16ToUTF8(WebIntentsInfo::GetIntentsServices(extension)[1].type));

  LoadAndExpectError("intent_invalid_type_element.json",
                     extension_manifest_errors::kInvalidIntentTypeElement);
}

TEST_F(WebIntentsManifestTest, WebIntentsInHostedApps) {
  Testcase testcases[] = {
    Testcase("intent_invalid_hosted_app_1.json",
             errors::kInvalidIntentPageInHostedApp),
    Testcase("intent_invalid_hosted_app_2.json",
             errors::kInvalidIntentPageInHostedApp),
    Testcase("intent_invalid_hosted_app_3.json",
             errors::kInvalidIntentPageInHostedApp)
  };
  RunTestcases(testcases, arraysize(testcases),
               EXPECT_TYPE_ERROR);

  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("intent_valid_hosted_app.json"));
  ASSERT_TRUE(extension);

  ASSERT_EQ(3u, WebIntentsInfo::GetIntentsServices(extension).size());
  EXPECT_EQ("http://www.cfp.com/intents/edit.html",
      WebIntentsInfo::GetIntentsServices(extension)[0].service_url.spec());
  EXPECT_EQ("http://www.cloudfilepicker.com/",
      WebIntentsInfo::GetIntentsServices(extension)[1].service_url.spec());
  EXPECT_EQ("http://www.cloudfilepicker.com/intents/share.html",
      WebIntentsInfo::GetIntentsServices(extension)[2].service_url.spec());
}

TEST_F(WebIntentsManifestTest, WebIntentsMultiHref) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("intent_valid_multi_href.json"));
  ASSERT_TRUE(extension);

  ASSERT_EQ(2u, WebIntentsInfo::GetIntentsServices(extension).size());
  EXPECT_EQ("chrome-extension",
      WebIntentsInfo::GetIntentsServices(extension)[0].service_url.scheme());
  EXPECT_EQ("/services/sharelink.html",
      WebIntentsInfo::GetIntentsServices(extension)[0].service_url.path());
  EXPECT_EQ("text/uri-list",
      UTF16ToUTF8(WebIntentsInfo::GetIntentsServices(extension)[0].type));
  EXPECT_EQ("chrome-extension",
      WebIntentsInfo::GetIntentsServices(extension)[1].service_url.scheme());
  EXPECT_EQ("/services/shareimage.html",
      WebIntentsInfo::GetIntentsServices(extension)[1].service_url.path());
  EXPECT_EQ("image/*",
      UTF16ToUTF8(WebIntentsInfo::GetIntentsServices(extension)[1].type));
}

TEST_F(WebIntentsManifestTest, WebIntentsBlankHref) {
  LoadAndExpectError("intent_invalid_blank_action_extension.json",
                     errors::kInvalidIntentHrefEmpty);

  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("intent_valid_blank_action_hosted.json"));
  ASSERT_TRUE(extension);

  ASSERT_EQ(1u, WebIntentsInfo::GetIntentsServices(extension).size());
  EXPECT_EQ("http://www.cloudfilepicker.com/",
      WebIntentsInfo::GetIntentsServices(extension)[0].service_url.spec());

  extension = LoadAndExpectSuccess("intent_valid_blank_action_packaged.json");
  ASSERT_TRUE(extension);

  ASSERT_EQ(1u, WebIntentsInfo::GetIntentsServices(extension).size());
  EXPECT_EQ("chrome-extension",
      WebIntentsInfo::GetIntentsServices(extension)[0].service_url.scheme());
  EXPECT_EQ("/main.html",
      WebIntentsInfo::GetIntentsServices(extension)[0].service_url.path());
}
