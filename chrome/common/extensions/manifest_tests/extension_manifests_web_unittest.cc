// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"

#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/web_intent_service_data.h"

namespace errors = extension_manifest_errors;

TEST_F(ExtensionManifestTest, WebAccessibleResources) {
  // Manifest version 2 with web accessible resources specified.
  scoped_refptr<Extension> extension1(
      LoadAndExpectSuccess("web_accessible_resources_1.json"));

  // Manifest version 2 with no web accessible resources.
  scoped_refptr<Extension> extension2(
      LoadAndExpectSuccess("web_accessible_resources_2.json"));

  // Default manifest version with web accessible resources specified.
  scoped_refptr<Extension> extension3(
      LoadAndExpectSuccess("web_accessible_resources_3.json"));

  // Default manifest version with no web accessible resources.
  scoped_refptr<Extension> extension4(
      LoadAndExpectSuccess("web_accessible_resources_4.json"));

  EXPECT_TRUE(extension1->HasWebAccessibleResources());
  EXPECT_FALSE(extension2->HasWebAccessibleResources());
  EXPECT_TRUE(extension3->HasWebAccessibleResources());
  EXPECT_FALSE(extension4->HasWebAccessibleResources());

  EXPECT_TRUE(extension1->IsResourceWebAccessible("/test"));
  EXPECT_FALSE(extension1->IsResourceWebAccessible("/none"));

  EXPECT_FALSE(extension2->IsResourceWebAccessible("/test"));

  EXPECT_TRUE(extension3->IsResourceWebAccessible("/test"));
  EXPECT_FALSE(extension3->IsResourceWebAccessible("/none"));

  EXPECT_TRUE(extension4->IsResourceWebAccessible("/test"));
  EXPECT_TRUE(extension4->IsResourceWebAccessible("/none"));
}

TEST_F(ExtensionManifestTest, WebIntents) {
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
  ASSERT_TRUE(extension.get() != NULL);

  ASSERT_EQ(1u, extension->intents_services().size());
  EXPECT_EQ("image/png", UTF16ToUTF8(extension->intents_services()[0].type));
  EXPECT_EQ("http://webintents.org/share",
            UTF16ToUTF8(extension->intents_services()[0].action));
  EXPECT_EQ("chrome-extension",
            extension->intents_services()[0].service_url.scheme());
  EXPECT_EQ("/services/share",
            extension->intents_services()[0].service_url.path());
  EXPECT_EQ("Sample Sharing Intent",
            UTF16ToUTF8(extension->intents_services()[0].title));
  EXPECT_EQ(webkit_glue::WebIntentServiceData::DISPOSITION_INLINE,
            extension->intents_services()[0].disposition);

  // Verify that optional fields are filled with defaults.
  extension = LoadAndExpectSuccess("intent_valid_minimal.json");
  ASSERT_TRUE(extension.get() != NULL);

  ASSERT_EQ(1u, extension->intents_services().size());
  EXPECT_EQ("*", UTF16ToUTF8(extension->intents_services()[0].type));
  EXPECT_EQ("http://webintents.org/share",
            UTF16ToUTF8(extension->intents_services()[0].action));
  EXPECT_EQ("", UTF16ToUTF8(extension->intents_services()[0].title));
  EXPECT_EQ(webkit_glue::WebIntentServiceData::DISPOSITION_WINDOW,
            extension->intents_services()[0].disposition);

  // Make sure we support href instead of path.
  extension = LoadAndExpectSuccess("intent_valid_using_href.json");
  ASSERT_TRUE(extension.get() != NULL);
  ASSERT_EQ(1u, extension->intents_services().size());
  EXPECT_EQ("/services/share",
      extension->intents_services()[0].service_url.path());
}

TEST_F(ExtensionManifestTest, WebIntentsWithMultipleMimeTypes) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("intent_valid_multitype.json"));
  ASSERT_TRUE(extension.get() != NULL);

  ASSERT_EQ(2u, extension->intents_services().size());

  // One registration with multiple types generates a separate service for
  // each MIME type.
  for (int i = 0; i < 2; ++i) {
    EXPECT_EQ("http://webintents.org/share",
              UTF16ToUTF8(extension->intents_services()[i].action));
    EXPECT_EQ("chrome-extension",
              extension->intents_services()[i].service_url.scheme());
    EXPECT_EQ("/services/share",
              extension->intents_services()[i].service_url.path());
    EXPECT_EQ("Sample Sharing Intent",
              UTF16ToUTF8(extension->intents_services()[i].title));
    EXPECT_EQ(webkit_glue::WebIntentServiceData::DISPOSITION_INLINE,
              extension->intents_services()[i].disposition);
  }
  EXPECT_EQ("image/jpeg", UTF16ToUTF8(extension->intents_services()[0].type));
  EXPECT_EQ("image/bmp", UTF16ToUTF8(extension->intents_services()[1].type));

  LoadAndExpectError("intent_invalid_type_element.json",
                     extension_manifest_errors::kInvalidIntentTypeElement);
}

TEST_F(ExtensionManifestTest, WebIntentsInHostedApps) {
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
  ASSERT_TRUE(extension.get() != NULL);

  ASSERT_EQ(3u, extension->intents_services().size());

  EXPECT_EQ("http://www.cfp.com/intents/edit.html",
            extension->intents_services()[0].service_url.spec());
  EXPECT_EQ("http://www.cloudfilepicker.com/",
            extension->intents_services()[1].service_url.spec());
  EXPECT_EQ("http://www.cloudfilepicker.com/intents/share.html",
            extension->intents_services()[2].service_url.spec());
}

TEST_F(ExtensionManifestTest, WebIntentsMultiHref) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("intent_valid_multi_href.json"));
  ASSERT_TRUE(extension.get() != NULL);
  ASSERT_EQ(2u, extension->intents_services().size());

  const std::vector<webkit_glue::WebIntentServiceData> &intents =
      extension->intents_services();

  EXPECT_EQ("chrome-extension", intents[0].service_url.scheme());
  EXPECT_EQ("/services/sharelink.html",intents[0].service_url.path());
  EXPECT_EQ("text/uri-list",UTF16ToUTF8(intents[0].type));

  EXPECT_EQ("chrome-extension", intents[1].service_url.scheme());
  EXPECT_EQ("/services/shareimage.html",intents[1].service_url.path());
  EXPECT_EQ("image/*",UTF16ToUTF8(intents[1].type));
}

TEST_F(ExtensionManifestTest, WebIntentsBlankHref) {
  LoadAndExpectError("intent_invalid_blank_action_extension.json",
                     errors::kInvalidIntentHrefEmpty);

  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("intent_valid_blank_action_hosted.json"));
  ASSERT_TRUE(extension.get() != NULL);
  ASSERT_EQ(1u, extension->intents_services().size());
  EXPECT_EQ("http://www.cloudfilepicker.com/",
      extension->intents_services()[0].service_url.spec());

  extension = LoadAndExpectSuccess("intent_valid_blank_action_packaged.json");
  ASSERT_TRUE(extension.get() != NULL);
  ASSERT_EQ(1u, extension->intents_services().size());
  EXPECT_EQ("chrome-extension",
      extension->intents_services()[0].service_url.scheme());
  EXPECT_EQ("/main.html",extension->intents_services()[0].service_url.path());
}

TEST_F(ExtensionManifestTest, AppWebUrls) {
  Testcase testcases[] = {
    Testcase("web_urls_wrong_type.json", errors::kInvalidWebURLs),
    Testcase("web_urls_invalid_1.json",
             ExtensionErrorUtils::FormatErrorMessage(
                 errors::kInvalidWebURL,
                 base::IntToString(0),
                 errors::kExpectString)),
    Testcase("web_urls_invalid_2.json",
             ExtensionErrorUtils::FormatErrorMessage(
                 errors::kInvalidWebURL,
                 base::IntToString(0),
                 URLPattern::GetParseResultString(
                 URLPattern::PARSE_ERROR_MISSING_SCHEME_SEPARATOR))),
    Testcase("web_urls_invalid_3.json",
             ExtensionErrorUtils::FormatErrorMessage(
                 errors::kInvalidWebURL,
                 base::IntToString(0),
                 errors::kNoWildCardsInPaths)),
    Testcase("web_urls_invalid_4.json",
             ExtensionErrorUtils::FormatErrorMessage(
                 errors::kInvalidWebURL,
                 base::IntToString(0),
                 errors::kCannotClaimAllURLsInExtent)),
    Testcase("web_urls_invalid_5.json",
             ExtensionErrorUtils::FormatErrorMessage(
                 errors::kInvalidWebURL,
                 base::IntToString(1),
                 errors::kCannotClaimAllHostsInExtent))
  };
  RunTestcases(testcases, arraysize(testcases),
               EXPECT_TYPE_ERROR);

  LoadAndExpectSuccess("web_urls_has_port.json");

  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("web_urls_default.json"));
  ASSERT_EQ(1u, extension->web_extent().patterns().size());
  EXPECT_EQ("*://www.google.com/*",
            extension->web_extent().patterns().begin()->GetAsString());
}
