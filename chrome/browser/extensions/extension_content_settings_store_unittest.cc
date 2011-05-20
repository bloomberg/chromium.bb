// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_content_settings_store.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Mock;

namespace {

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
  MOCK_METHOD0(OnDestruction, void());
};

}

TEST(ExtensionContentSettingsStoreTest, RegisterUnregister) {
  FakeTimer timer;

  ::testing::StrictMock<MockExtensionContentSettingsStoreObserver> observer;

  ExtensionContentSettingsStore map;
  map.AddObserver(&observer);

  GURL url("http://www.youtube.com");

  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            map.GetEffectiveContentSetting(
                url,
                url,
                CONTENT_SETTINGS_TYPE_COOKIES,
                "",
                false));

  // Register first extension
  std::string ext_id("my_extension");
  base::Time time_1 = timer.GetNext();
  map.RegisterExtension(ext_id, time_1, true);

  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            map.GetEffectiveContentSetting(
                url,
                url,
                CONTENT_SETTINGS_TYPE_COOKIES,
                "",
                false));

  // Set setting
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString("http://www.youtube.com");
  EXPECT_CALL(observer, OnContentSettingChanged(ext_id, false));
  map.SetExtensionContentSetting(
      ext_id,
      pattern,
      pattern,
      CONTENT_SETTINGS_TYPE_COOKIES,
      "",
      CONTENT_SETTING_ALLOW,
      false);
  Mock::VerifyAndClear(&observer);

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map.GetEffectiveContentSetting(
                url,
                url,
                CONTENT_SETTINGS_TYPE_COOKIES,
                "",
                false));

  // Register second extension.
  std::string ext_id_2("my_second_extension");
  base::Time time_2 = timer.GetNext();
  map.RegisterExtension(ext_id_2, time_2, true);
  EXPECT_CALL(observer, OnContentSettingChanged(ext_id_2, false));
  map.SetExtensionContentSetting(
      ext_id_2,
      pattern,
      pattern,
      CONTENT_SETTINGS_TYPE_COOKIES,
      "",
      CONTENT_SETTING_BLOCK,
      false);

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            map.GetEffectiveContentSetting(
                url,
                url,
                CONTENT_SETTINGS_TYPE_COOKIES,
                "",
                false));

  // Unregister first extension. This shouldn't change the setting.
  EXPECT_CALL(observer, OnContentSettingChanged(ext_id, false));
  map.UnregisterExtension(ext_id);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            map.GetEffectiveContentSetting(
                url,
                url,
                CONTENT_SETTINGS_TYPE_COOKIES,
                "",
                false));
  Mock::VerifyAndClear(&observer);

  // Unregister second extension. This should reset the setting to its default
  // value.
  EXPECT_CALL(observer, OnContentSettingChanged(ext_id_2, false));
  map.UnregisterExtension(ext_id_2);
  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            map.GetEffectiveContentSetting(
                url,
                url,
                CONTENT_SETTINGS_TYPE_COOKIES,
                "",
                false));
  Mock::VerifyAndClear(&observer);

  EXPECT_CALL(observer, OnDestruction());
}
