// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/site_settings_handler.h"

#include <memory>

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/webui/site_settings_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_web_ui.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#endif

namespace {

const char kCallbackId[] = "test-callback-id";
const char kSetting[] = "setting";
const char kSource[] = "source";

}

namespace settings {

class SiteSettingsHandlerTest : public testing::Test {
 public:
  SiteSettingsHandlerTest() : handler_(&profile_) {
#if defined(OS_CHROMEOS)
    mock_user_manager_ = new chromeos::MockUserManager;
    user_manager_enabler_.reset(
        new chromeos::ScopedUserManagerEnabler(mock_user_manager_));
#endif
  }

  void SetUp() override {
    handler()->set_web_ui(web_ui());
    handler()->AllowJavascript();
    web_ui()->ClearTrackedCalls();
  }

  Profile* profile() { return &profile_; }
  content::TestWebUI* web_ui() { return &web_ui_; }
  SiteSettingsHandler* handler() { return &handler_; }

  void ValidateDefault(const std::string& expected_setting,
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

    const base::DictionaryValue* default_value = nullptr;
    ASSERT_TRUE(data.arg3()->GetAsDictionary(&default_value));
    std::string setting;
    ASSERT_TRUE(default_value->GetString(kSetting, &setting));
    EXPECT_EQ(expected_setting, setting);
    std::string source;
    if (default_value->GetString(kSource, &source))
      EXPECT_EQ(expected_source, source);
  }

  void ValidateOrigin(
      const std::string& expected_origin,
      const std::string& expected_display_name,
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
    std::string origin, embedding_origin, display_name, setting, source;
    ASSERT_TRUE(exception->GetString(site_settings::kOrigin, &origin));
    ASSERT_EQ(expected_origin, origin);
    ASSERT_TRUE(
        exception->GetString(site_settings::kDisplayName, &display_name));
    ASSERT_EQ(expected_display_name, display_name);
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

  void ValidateIncognitoExists(
      bool expected_incognito, size_t expected_total_calls) {
    EXPECT_EQ(expected_total_calls, web_ui()->call_data().size());

    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIListenerCallback", data.function_name());

    std::string callback_id;
    ASSERT_TRUE(data.arg1()->GetAsString(&callback_id));
    EXPECT_EQ("onIncognitoStatusChanged", callback_id);

    bool incognito;
    ASSERT_TRUE(data.arg2()->GetAsBoolean(&incognito));
    EXPECT_EQ(expected_incognito, incognito);
  }

  void ValidateZoom(const std::string& expected_host,
      const std::string& expected_zoom, size_t expected_total_calls) {
    EXPECT_EQ(expected_total_calls, web_ui()->call_data().size());

    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIListenerCallback", data.function_name());

    std::string callback_id;
    ASSERT_TRUE(data.arg1()->GetAsString(&callback_id));
    EXPECT_EQ("onZoomLevelsChanged", callback_id);

    const base::ListValue* exceptions;
    ASSERT_TRUE(data.arg2()->GetAsList(&exceptions));
    if (expected_host.empty()) {
      EXPECT_EQ(0U, exceptions->GetSize());
    } else {
      EXPECT_EQ(1U, exceptions->GetSize());

      const base::DictionaryValue* exception;
      ASSERT_TRUE(exceptions->GetDictionary(0, &exception));

      std::string host;
      ASSERT_TRUE(exception->GetString("origin", &host));
      ASSERT_EQ(expected_host, host);

      std::string zoom;
      ASSERT_TRUE(exception->GetString("zoom", &zoom));
      ASSERT_EQ(expected_zoom, zoom);
    }
  }

  void CreateIncognitoProfile() {
    incognito_profile_ = TestingProfile::Builder().BuildIncognito(&profile_);
  }

  void DestroyIncognitoProfile() {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_PROFILE_DESTROYED,
        content::Source<Profile>(static_cast<Profile*>(incognito_profile_)),
        content::NotificationService::NoDetails());
    profile_.SetOffTheRecordProfile(nullptr);
    ASSERT_FALSE(profile_.HasOffTheRecordProfile());
    incognito_profile_ = nullptr;
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  TestingProfile* incognito_profile_;
  content::TestWebUI web_ui_;
  SiteSettingsHandler handler_;
#if defined(OS_CHROMEOS)
  chromeos::MockUserManager* mock_user_manager_;  // Not owned.
  std::unique_ptr<chromeos::ScopedUserManagerEnabler> user_manager_enabler_;
#endif
};

TEST_F(SiteSettingsHandlerTest, GetAndSetDefault) {
  // Test the JS -> C++ -> JS callback path for getting and setting defaults.
  base::ListValue getArgs;
  getArgs.AppendString(kCallbackId);
  getArgs.AppendString("notifications");
  handler()->HandleGetDefaultValueForContentType(&getArgs);
  ValidateDefault("ask", "default", 1U);

  // Set the default to 'Blocked'.
  base::ListValue setArgs;
  setArgs.AppendString("notifications");
  setArgs.AppendString("block");
  handler()->HandleSetDefaultValueForContentType(&setArgs);

  EXPECT_EQ(2U, web_ui()->call_data().size());

  // Verify that the default has been set to 'Blocked'.
  handler()->HandleGetDefaultValueForContentType(&getArgs);
  ValidateDefault("block", "default", 3U);
}

TEST_F(SiteSettingsHandlerTest, Origins) {
  // Test the JS -> C++ -> JS callback path for configuring origins, by setting
  // Google.com to blocked.
  base::ListValue setArgs;
  std::string google("http://www.google.com");
  setArgs.AppendString(google);  // Primary pattern.
  setArgs.AppendString(google);  // Secondary pattern.
  setArgs.AppendString("notifications");
  setArgs.AppendString("block");
  setArgs.AppendBoolean(false);  // Incognito.
  handler()->HandleSetCategoryPermissionForOrigin(&setArgs);
  EXPECT_EQ(1U, web_ui()->call_data().size());

  // Verify the change was successful.
  base::ListValue listArgs;
  listArgs.AppendString(kCallbackId);
  listArgs.AppendString("notifications");
  handler()->HandleGetExceptionList(&listArgs);
  ValidateOrigin(google, google, google, "block", "preference", 2U);

  // Reset things back to how they were.
  base::ListValue resetArgs;
  resetArgs.AppendString(google);
  resetArgs.AppendString(google);
  resetArgs.AppendString("notifications");
  resetArgs.AppendBoolean(false);  // Incognito.
  handler()->HandleResetCategoryPermissionForOrigin(&resetArgs);
  EXPECT_EQ(3U, web_ui()->call_data().size());

  // Verify the reset was successful.
  handler()->HandleGetExceptionList(&listArgs);
  ValidateNoOrigin(4U);
}

TEST_F(SiteSettingsHandlerTest, ExceptionHelpers) {
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString("[*.]google.com");
  std::unique_ptr<base::DictionaryValue> exception =
      site_settings::GetExceptionForPage(pattern, pattern, pattern.ToString(),
                                         CONTENT_SETTING_BLOCK, "preference",
                                         false);

  std::string primary_pattern, secondary_pattern, display_name, type;
  bool incognito;
  CHECK(exception->GetString(site_settings::kOrigin, &primary_pattern));
  CHECK(exception->GetString(site_settings::kDisplayName, &display_name));
  CHECK(exception->GetString(site_settings::kEmbeddingOrigin,
                             &secondary_pattern));
  CHECK(exception->GetString(site_settings::kSetting, &type));
  CHECK(exception->GetBoolean(site_settings::kIncognito, &incognito));

  base::ListValue args;
  args.AppendString(primary_pattern);
  args.AppendString(secondary_pattern);
  args.AppendString("notifications");  // Chosen arbitrarily.
  args.AppendString(type);
  args.AppendBoolean(incognito);

  // We don't need to check the results. This is just to make sure it doesn't
  // crash on the input.
  handler()->HandleSetCategoryPermissionForOrigin(&args);

  scoped_refptr<const extensions::Extension> extension;
  extension = extensions::ExtensionBuilder()
                  .SetManifest(extensions::DictionaryBuilder()
                                   .Set("name", "Test extension")
                                   .Set("version", "1.0.0")
                                   .Set("manifest_version", 2)
                                   .Build())
                  .SetID("ahfgeienlihckogmohjhadlkjgocpleb")
                  .Build();

  std::unique_ptr<base::ListValue> exceptions(new base::ListValue);
  site_settings::AddExceptionForHostedApp(
      "[*.]google.com", *extension.get(), exceptions.get());

  const base::DictionaryValue* dictionary;
  CHECK(exceptions->GetDictionary(0, &dictionary));
  CHECK(dictionary->GetString(site_settings::kOrigin, &primary_pattern));
  CHECK(dictionary->GetString(site_settings::kDisplayName, &display_name));
  CHECK(dictionary->GetString(site_settings::kEmbeddingOrigin,
                              &secondary_pattern));
  CHECK(dictionary->GetString(site_settings::kSetting, &type));
  CHECK(dictionary->GetBoolean(site_settings::kIncognito, &incognito));

  // Again, don't need to check the results.
  handler()->HandleSetCategoryPermissionForOrigin(&args);
}

TEST_F(SiteSettingsHandlerTest, Patterns) {
  base::ListValue args;
  std::string pattern("[*.]google.com");
  args.AppendString(kCallbackId);
  args.AppendString(pattern);
  handler()->HandleIsPatternValid(&args);
  ValidatePattern(true, 1U);

  base::ListValue invalid;
  std::string bad_pattern(";");
  invalid.AppendString(kCallbackId);
  invalid.AppendString(bad_pattern);
  handler()->HandleIsPatternValid(&invalid);
  ValidatePattern(false, 2U);
}

TEST_F(SiteSettingsHandlerTest, Incognito) {
  base::ListValue args;
  handler()->HandleUpdateIncognitoStatus(&args);
  ValidateIncognitoExists(false, 1U);

  CreateIncognitoProfile();
  ValidateIncognitoExists(true, 2U);

  DestroyIncognitoProfile();
  ValidateIncognitoExists(false, 3U);
}

TEST_F(SiteSettingsHandlerTest, ZoomLevels) {
  std::string host("http://www.google.com");
  double zoom_level = 1.1;

  content::HostZoomMap* host_zoom_map =
      content::HostZoomMap::GetDefaultForBrowserContext(profile());
  host_zoom_map->SetZoomLevelForHost(host, zoom_level);
  ValidateZoom(host, "122%", 1U);

  base::ListValue args;
  handler()->HandleFetchZoomLevels(&args);
  ValidateZoom(host, "122%", 2U);

  args.AppendString("http://www.google.com");
  handler()->HandleRemoveZoomLevel(&args);
  ValidateZoom("", "", 3U);

  double default_level = host_zoom_map->GetDefaultZoomLevel();
  double level = host_zoom_map->GetZoomLevelForHostAndScheme("http", host);
  EXPECT_EQ(default_level, level);
}

}  // namespace settings
