// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_content_settings_store.h"

#include "base/scoped_ptr.h"
#include "chrome/browser/content_settings/content_settings_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Mock;

namespace {

void CheckRule(const content_settings::ProviderInterface::Rule& rule,
               const ContentSettingsPattern& primary_pattern,
               const ContentSettingsPattern& secondary_pattern,
               ContentSetting setting) {
  EXPECT_EQ(primary_pattern.ToString(), rule.primary_pattern.ToString());
  EXPECT_EQ(secondary_pattern.ToString(), rule.secondary_pattern.ToString());
  EXPECT_EQ(setting, rule.content_setting);
}

// Takes ownership of |value|.
ContentSetting ValueToContentSetting(base::Value* value) {
  scoped_ptr<base::Value> owned_value(value);
  ContentSetting setting = CONTENT_SETTING_DEFAULT;
  EXPECT_TRUE(content_settings::ParseContentSettingValue(value, &setting));
  return setting;
}

// Helper class which returns monotonically-increasing base::Time objects.
class FakeTimer {
 public:
  FakeTimer() : internal_(0) {}

  base::Time GetNext() {
    return base::Time::FromInternalValue(++internal_);
  }

 private:
  int64 internal_;
};

class MockExtensionContentSettingsStoreObserver
    : public ExtensionContentSettingsStore::Observer {
 public:
  MOCK_METHOD2(OnContentSettingChanged,
               void(const std::string& extension_id, bool incognito));
};

}

class ExtensionContentSettingsStoreTest : public ::testing::Test {
 public:
  ExtensionContentSettingsStoreTest() :
      store_(new ExtensionContentSettingsStore()) {
  }

 protected:
  void RegisterExtension(const std::string& ext_id) {
    store_->RegisterExtension(ext_id, timer_.GetNext(), true);
  }

  ExtensionContentSettingsStore* store() {
    return store_.get();
  }

 private:
  FakeTimer timer_;
  scoped_refptr<ExtensionContentSettingsStore> store_;
};

TEST_F(ExtensionContentSettingsStoreTest, RegisterUnregister) {
  ::testing::StrictMock<MockExtensionContentSettingsStoreObserver> observer;
  store()->AddObserver(&observer);

  GURL url("http://www.youtube.com");

  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            ValueToContentSetting(store()->GetEffectiveContentSetting(
                url, url, CONTENT_SETTINGS_TYPE_COOKIES, "", false)));

  // Register first extension
  std::string ext_id("my_extension");
  RegisterExtension(ext_id);

  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            ValueToContentSetting(store()->GetEffectiveContentSetting(
                url, url, CONTENT_SETTINGS_TYPE_COOKIES, "", false)));

  // Set setting
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromURL(GURL("http://www.youtube.com"));
  EXPECT_CALL(observer, OnContentSettingChanged(ext_id, false));
  store()->SetExtensionContentSetting(
      ext_id,
      pattern,
      pattern,
      CONTENT_SETTINGS_TYPE_COOKIES,
      "",
      CONTENT_SETTING_ALLOW,
      kExtensionPrefsScopeRegular);
  Mock::VerifyAndClear(&observer);

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            ValueToContentSetting(store()->GetEffectiveContentSetting(
                url,
                url,
                CONTENT_SETTINGS_TYPE_COOKIES,
                "",
                false)));

  // Register second extension.
  std::string ext_id_2("my_second_extension");
  RegisterExtension(ext_id_2);
  EXPECT_CALL(observer, OnContentSettingChanged(ext_id_2, false));
  store()->SetExtensionContentSetting(
      ext_id_2,
      pattern,
      pattern,
      CONTENT_SETTINGS_TYPE_COOKIES,
      "",
      CONTENT_SETTING_BLOCK,
      kExtensionPrefsScopeRegular);

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            ValueToContentSetting(store()->GetEffectiveContentSetting(
                url,
                url,
                CONTENT_SETTINGS_TYPE_COOKIES,
                "",
                false)));

  // Unregister first extension. This shouldn't change the setting.
  EXPECT_CALL(observer, OnContentSettingChanged(ext_id, false));
  store()->UnregisterExtension(ext_id);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            ValueToContentSetting(store()->GetEffectiveContentSetting(
                url,
                url,
                CONTENT_SETTINGS_TYPE_COOKIES,
                "",
                false)));
  Mock::VerifyAndClear(&observer);

  // Unregister second extension. This should reset the setting to its default
  // value.
  EXPECT_CALL(observer, OnContentSettingChanged(ext_id_2, false));
  store()->UnregisterExtension(ext_id_2);
  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            ValueToContentSetting(store()->GetEffectiveContentSetting(
                url,
                url,
                CONTENT_SETTINGS_TYPE_COOKIES,
                "",
                false)));

  store()->RemoveObserver(&observer);
}

TEST_F(ExtensionContentSettingsStoreTest, GetAllSettings) {
  bool incognito = false;
  content_settings::ProviderInterface::Rules rules;
  store()->GetContentSettingsForContentType(
      CONTENT_SETTINGS_TYPE_COOKIES, "", incognito, &rules);
  ASSERT_EQ(0u, rules.size());

  // Register first extension.
  std::string ext_id("my_extension");
  RegisterExtension(ext_id);
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromURL(GURL("http://www.youtube.com"));
  store()->SetExtensionContentSetting(
      ext_id,
      pattern,
      pattern,
      CONTENT_SETTINGS_TYPE_COOKIES,
      "",
      CONTENT_SETTING_ALLOW,
      kExtensionPrefsScopeRegular);

  store()->GetContentSettingsForContentType(
      CONTENT_SETTINGS_TYPE_COOKIES, "", incognito, &rules);
  ASSERT_EQ(1u, rules.size());
  CheckRule(rules[0], pattern, pattern, CONTENT_SETTING_ALLOW);

  // Register second extension.
  std::string ext_id_2("my_second_extension");
  RegisterExtension(ext_id_2);
  ContentSettingsPattern pattern_2 =
      ContentSettingsPattern::FromURL(GURL("http://www.example.com"));
  store()->SetExtensionContentSetting(
      ext_id_2,
      pattern_2,
      pattern_2,
      CONTENT_SETTINGS_TYPE_COOKIES,
      "",
      CONTENT_SETTING_BLOCK,
      kExtensionPrefsScopeRegular);

  rules.clear();
  store()->GetContentSettingsForContentType(
      CONTENT_SETTINGS_TYPE_COOKIES, "", incognito, &rules);
  ASSERT_EQ(2u, rules.size());
  CheckRule(rules[0], pattern, pattern, CONTENT_SETTING_ALLOW);
  CheckRule(rules[1], pattern_2, pattern_2, CONTENT_SETTING_BLOCK);

  // Disable first extension.
  store()->SetExtensionState(ext_id, false);

  rules.clear();
  store()->GetContentSettingsForContentType(
      CONTENT_SETTINGS_TYPE_COOKIES, "", incognito, &rules);
  ASSERT_EQ(1u, rules.size());
  CheckRule(rules[0], pattern_2, pattern_2, CONTENT_SETTING_BLOCK);

  // Uninstall second extension.
  store()->UnregisterExtension(ext_id_2);

  rules.clear();
  store()->GetContentSettingsForContentType(
      CONTENT_SETTINGS_TYPE_COOKIES, "", incognito, &rules);
  ASSERT_EQ(0u, rules.size());
}
