// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/site_settings_handler.h"

#include <memory>

#include "chrome/browser/ui/webui/site_settings_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kCallbackId[] = "test-callback-id";

}

namespace settings {

class SiteSettingsHandlerTest : public testing::Test {
 public:
  SiteSettingsHandlerTest() {}

  Profile* profile() { return &profile_; }
  content::TestWebUI* web_ui() { return &web_ui_; }

  void ValidateDefault(bool expected_default, size_t expected_total_calls) {
    EXPECT_EQ(expected_total_calls, web_ui()->call_data().size());

    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIResponse", data.function_name());

    std::string callback_id;
    ASSERT_TRUE(data.arg1()->GetAsString(&callback_id));
    EXPECT_EQ(kCallbackId, callback_id);

    bool success = false;
    ASSERT_TRUE(data.arg2()->GetAsBoolean(&success));
    ASSERT_TRUE(success);

    bool enabled;
    ASSERT_TRUE(data.arg3()->GetAsBoolean(&enabled));
    EXPECT_EQ(expected_default, enabled);
  }

  void ValidateOrigin(
      const std::string& expected_origin,
      const std::string& expected_embedding,
      const std::string& expected_setting,
      const std::string& expected_source,
      size_t expected_total_calls) {
    EXPECT_EQ(expected_total_calls, web_ui()->call_data().size());

    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIResponse", data.function_name());

    std::string callback_id;
    ASSERT_TRUE(data.arg1()->GetAsString(&callback_id));
    EXPECT_EQ(kCallbackId, callback_id);
    bool success = false;
    ASSERT_TRUE(data.arg2()->GetAsBoolean(&success));
    ASSERT_TRUE(success);

    const base::ListValue* exceptions;
    ASSERT_TRUE(data.arg3()->GetAsList(&exceptions));
    EXPECT_EQ(1U, exceptions->GetSize());
    const base::DictionaryValue* exception;
    ASSERT_TRUE(exceptions->GetDictionary(0, &exception));
    std::string origin, embedding_origin, setting, source;
    ASSERT_TRUE(exception->GetString(site_settings::kOrigin, &origin));
    ASSERT_EQ(expected_origin, origin);
    ASSERT_TRUE(exception->GetString(
        site_settings::kEmbeddingOrigin, &embedding_origin));
    ASSERT_EQ(expected_embedding, embedding_origin);
    ASSERT_TRUE(exception->GetString(site_settings::kSetting, &setting));
    ASSERT_EQ(expected_setting, setting);
    ASSERT_TRUE(exception->GetString(site_settings::kSource, &source));
    ASSERT_EQ(expected_source, source);
  }

  void ValidateNoOrigin(size_t expected_total_calls) {
    EXPECT_EQ(expected_total_calls, web_ui()->call_data().size());

    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIResponse", data.function_name());

    std::string callback_id;
    ASSERT_TRUE(data.arg1()->GetAsString(&callback_id));
    EXPECT_EQ(kCallbackId, callback_id);

    bool success = false;
    ASSERT_TRUE(data.arg2()->GetAsBoolean(&success));
    ASSERT_TRUE(success);

    const base::ListValue* exceptions;
    ASSERT_TRUE(data.arg3()->GetAsList(&exceptions));
    EXPECT_EQ(0U, exceptions->GetSize());
  }

  void ValidatePattern(bool expected_validity, size_t expected_total_calls) {
    EXPECT_EQ(expected_total_calls, web_ui()->call_data().size());

    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIResponse", data.function_name());

    std::string callback_id;
    ASSERT_TRUE(data.arg1()->GetAsString(&callback_id));
    EXPECT_EQ(kCallbackId, callback_id);

    bool success = false;
    ASSERT_TRUE(data.arg2()->GetAsBoolean(&success));
    ASSERT_TRUE(success);

    bool valid;
    ASSERT_TRUE(data.arg3()->GetAsBoolean(&valid));
    EXPECT_EQ(expected_validity, valid);
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  content::TestWebUI web_ui_;
};

TEST_F(SiteSettingsHandlerTest, GetAndSetDefault) {
  SiteSettingsHandler handler(profile());
  handler.set_web_ui(web_ui());

  ContentSettingsType type = CONTENT_SETTINGS_TYPE_NOTIFICATIONS;

  // Test the JS -> C++ -> JS callback path for getting and setting defaults.
  base::ListValue getArgs;
  getArgs.Append(new base::StringValue(kCallbackId));
  getArgs.Append(new base::FundamentalValue(type));
  handler.HandleGetDefaultValueForContentType(&getArgs);
  ValidateDefault(true, 1U);

  // Set the default to 'Blocked'.
  base::ListValue setArgs;
  setArgs.Append(new base::FundamentalValue(type));
  setArgs.Append(new base::StringValue("block"));
  handler.HandleSetDefaultValueForContentType(&setArgs);

  EXPECT_EQ(2U, web_ui()->call_data().size());

  // Verify that the default has been set to 'Blocked'.
  handler.HandleGetDefaultValueForContentType(&getArgs);
  ValidateDefault(false, 3U);
}

TEST_F(SiteSettingsHandlerTest, Origins) {
  SiteSettingsHandler handler(profile());
  handler.set_web_ui(web_ui());

  ContentSettingsType type = CONTENT_SETTINGS_TYPE_NOTIFICATIONS;

  // Test the JS -> C++ -> JS callback path for configuring origins, by setting
  // Google.com to blocked.
  base::ListValue setArgs;
  std::string google("http://www.google.com");
  setArgs.Append(new base::StringValue(google));  // Primary pattern.
  setArgs.Append(new base::StringValue(google));  // Secondary pattern.
  setArgs.Append(new base::FundamentalValue(type));
  setArgs.Append(new base::StringValue("block"));
  handler.HandleSetCategoryPermissionForOrigin(&setArgs);
  EXPECT_EQ(1U, web_ui()->call_data().size());

  // Verify the change was successful.
  base::ListValue listArgs;
  listArgs.Append(new base::StringValue(kCallbackId));
  listArgs.Append(new base::FundamentalValue(type));
  handler.HandleGetExceptionList(&listArgs);
  ValidateOrigin(google, google, "block", "preference", 2U);

  // Reset things back to how they were.
  base::ListValue resetArgs;
  resetArgs.Append(new base::StringValue(google));
  resetArgs.Append(new base::StringValue(google));
  resetArgs.Append(new base::FundamentalValue(type));
  handler.HandleResetCategoryPermissionForOrigin(&resetArgs);
  EXPECT_EQ(3U, web_ui()->call_data().size());

  // Verify the reset was successful.
  handler.HandleGetExceptionList(&listArgs);
  ValidateNoOrigin(4U);
}

TEST_F(SiteSettingsHandlerTest, Patterns) {
  SiteSettingsHandler handler(profile());
  handler.set_web_ui(web_ui());

  base::ListValue args;
  std::string pattern("[*.]google.com");
  args.Append(new base::StringValue(kCallbackId));
  args.Append(new base::StringValue(pattern));
  handler.HandleIsPatternValid(&args);
  ValidatePattern(true, 1U);

  base::ListValue invalid;
  std::string bad_pattern(";");
  invalid.Append(new base::StringValue(kCallbackId));
  invalid.Append(new base::StringValue(bad_pattern));
  handler.HandleIsPatternValid(&invalid);
  ValidatePattern(false, 2U);
}

}  // namespace settings
