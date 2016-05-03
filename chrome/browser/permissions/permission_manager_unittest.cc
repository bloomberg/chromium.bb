// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_manager.h"

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/permission_type.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

using permissions::mojom::PermissionStatus;
using content::PermissionType;

namespace {

class PermissionManagerTestingProfile final : public TestingProfile {
 public:
  PermissionManagerTestingProfile() {}
  ~PermissionManagerTestingProfile() override {}

  PermissionManager* GetPermissionManager() override {
    return PermissionManagerFactory::GetForProfile(this);
  }

  DISALLOW_COPY_AND_ASSIGN(PermissionManagerTestingProfile);
};

}  // anonymous namespace

class PermissionManagerTest : public testing::Test {
 public:
  void OnPermissionChange(PermissionStatus permission) {
    callback_called_ = true;
    callback_result_ = permission;
  }

 protected:
  PermissionManagerTest()
      : url_("https://example.com"),
        other_url_("https://foo.com"),
        callback_called_(false),
        callback_result_(PermissionStatus::ASK) {}

  PermissionManager* GetPermissionManager() {
    return profile_.GetPermissionManager();
  }

  HostContentSettingsMap* GetHostContentSettingsMap() {
    return HostContentSettingsMapFactory::GetForProfile(&profile_);
  }

  void CheckPermissionStatus(PermissionType type,
                             PermissionStatus expected) {
    EXPECT_EQ(expected, GetPermissionManager()->GetPermissionStatus(
                            type, url_.GetOrigin(), url_.GetOrigin()));
  }

  void SetPermission(ContentSettingsType type, ContentSetting value) {
    HostContentSettingsMapFactory::GetForProfile(&profile_)
        ->SetContentSettingDefaultScope(url_, url_, type, std::string(), value);
  }

  const GURL& url() const {
    return url_;
  }

  const GURL& other_url() const {
    return other_url_;
  }

  bool callback_called() const {
    return callback_called_;
  }

  PermissionStatus callback_result() const { return callback_result_; }

  void Reset() {
    callback_called_ = false;
    callback_result_ = PermissionStatus::ASK;
  }

 private:
  const GURL url_;
  const GURL other_url_;
  bool callback_called_;
  PermissionStatus callback_result_;
  content::TestBrowserThreadBundle thread_bundle_;
  PermissionManagerTestingProfile profile_;
};

TEST_F(PermissionManagerTest, GetPermissionStatusDefault) {
  CheckPermissionStatus(PermissionType::MIDI_SYSEX, PermissionStatus::ASK);
  CheckPermissionStatus(PermissionType::PUSH_MESSAGING, PermissionStatus::ASK);
  CheckPermissionStatus(PermissionType::NOTIFICATIONS, PermissionStatus::ASK);
  CheckPermissionStatus(PermissionType::GEOLOCATION, PermissionStatus::ASK);
#if defined(OS_ANDROID)
  CheckPermissionStatus(PermissionType::PROTECTED_MEDIA_IDENTIFIER,
                        PermissionStatus::ASK);
#endif
}

TEST_F(PermissionManagerTest, GetPermissionStatusAfterSet) {
  SetPermission(CONTENT_SETTINGS_TYPE_GEOLOCATION, CONTENT_SETTING_ALLOW);
  CheckPermissionStatus(PermissionType::GEOLOCATION, PermissionStatus::GRANTED);

  SetPermission(CONTENT_SETTINGS_TYPE_NOTIFICATIONS, CONTENT_SETTING_ALLOW);
  CheckPermissionStatus(PermissionType::NOTIFICATIONS,
                        PermissionStatus::GRANTED);

  SetPermission(CONTENT_SETTINGS_TYPE_MIDI_SYSEX, CONTENT_SETTING_ALLOW);
  CheckPermissionStatus(PermissionType::MIDI_SYSEX, PermissionStatus::GRANTED);

  SetPermission(CONTENT_SETTINGS_TYPE_PUSH_MESSAGING, CONTENT_SETTING_ALLOW);
  CheckPermissionStatus(PermissionType::PUSH_MESSAGING,
                        PermissionStatus::GRANTED);

#if defined(OS_ANDROID)
  SetPermission(CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER,
                CONTENT_SETTING_ALLOW);
  CheckPermissionStatus(PermissionType::PROTECTED_MEDIA_IDENTIFIER,
                        PermissionStatus::GRANTED);
#endif
}

TEST_F(PermissionManagerTest, SameTypeChangeNotifies) {
  int subscription_id = GetPermissionManager()->SubscribePermissionStatusChange(
      PermissionType::GEOLOCATION, url(), url(),
      base::Bind(&PermissionManagerTest::OnPermissionChange,
                 base::Unretained(this)));

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string(),
      CONTENT_SETTING_ALLOW);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::GRANTED, callback_result());

  GetPermissionManager()->UnsubscribePermissionStatusChange(subscription_id);
}

TEST_F(PermissionManagerTest, DifferentTypeChangeDoesNotNotify) {
  int subscription_id = GetPermissionManager()->SubscribePermissionStatusChange(
      PermissionType::GEOLOCATION, url(), url(),
      base::Bind(&PermissionManagerTest::OnPermissionChange,
                 base::Unretained(this)));

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), GURL(), CONTENT_SETTINGS_TYPE_NOTIFICATIONS, std::string(),
      CONTENT_SETTING_ALLOW);

  EXPECT_FALSE(callback_called());

  GetPermissionManager()->UnsubscribePermissionStatusChange(subscription_id);
}

TEST_F(PermissionManagerTest, ChangeAfterUnsubscribeDoesNotNotify) {
  int subscription_id = GetPermissionManager()->SubscribePermissionStatusChange(
      PermissionType::GEOLOCATION, url(), url(),
      base::Bind(&PermissionManagerTest::OnPermissionChange,
                 base::Unretained(this)));

  GetPermissionManager()->UnsubscribePermissionStatusChange(subscription_id);

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string(),
      CONTENT_SETTING_ALLOW);

  EXPECT_FALSE(callback_called());
}

TEST_F(PermissionManagerTest, DifferentPrimaryUrlDoesNotNotify) {
  int subscription_id = GetPermissionManager()->SubscribePermissionStatusChange(
      PermissionType::GEOLOCATION, url(), url(),
      base::Bind(&PermissionManagerTest::OnPermissionChange,
                 base::Unretained(this)));

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      other_url(), url(), CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string(),
      CONTENT_SETTING_ALLOW);

  EXPECT_FALSE(callback_called());

  GetPermissionManager()->UnsubscribePermissionStatusChange(subscription_id);
}

TEST_F(PermissionManagerTest, DifferentSecondaryUrlDoesNotNotify) {
  int subscription_id = GetPermissionManager()->SubscribePermissionStatusChange(
      PermissionType::GEOLOCATION, url(), url(),
      base::Bind(&PermissionManagerTest::OnPermissionChange,
                 base::Unretained(this)));

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), other_url(), CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string(),
      CONTENT_SETTING_ALLOW);

  EXPECT_FALSE(callback_called());

  GetPermissionManager()->UnsubscribePermissionStatusChange(subscription_id);
}

TEST_F(PermissionManagerTest, WildCardPatternNotifies) {
  int subscription_id = GetPermissionManager()->SubscribePermissionStatusChange(
      PermissionType::GEOLOCATION, url(), url(),
      base::Bind(&PermissionManagerTest::OnPermissionChange,
                 base::Unretained(this)));

  GetHostContentSettingsMap()->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_GEOLOCATION, CONTENT_SETTING_ALLOW);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::GRANTED, callback_result());

  GetPermissionManager()->UnsubscribePermissionStatusChange(subscription_id);
}

TEST_F(PermissionManagerTest, ClearSettingsNotifies) {
  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string(),
      CONTENT_SETTING_ALLOW);

  int subscription_id = GetPermissionManager()->SubscribePermissionStatusChange(
      PermissionType::GEOLOCATION, url(), url(),
      base::Bind(&PermissionManagerTest::OnPermissionChange,
                 base::Unretained(this)));

  GetHostContentSettingsMap()->ClearSettingsForOneType(
      CONTENT_SETTINGS_TYPE_GEOLOCATION);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::ASK, callback_result());

  GetPermissionManager()->UnsubscribePermissionStatusChange(subscription_id);
}

TEST_F(PermissionManagerTest, NewValueCorrectlyPassed) {
  int subscription_id = GetPermissionManager()->SubscribePermissionStatusChange(
      PermissionType::GEOLOCATION, url(), url(),
      base::Bind(&PermissionManagerTest::OnPermissionChange,
                 base::Unretained(this)));

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string(),
      CONTENT_SETTING_BLOCK);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::DENIED, callback_result());

  GetPermissionManager()->UnsubscribePermissionStatusChange(subscription_id);
}

TEST_F(PermissionManagerTest, ChangeWithoutPermissionChangeDoesNotNotify) {
  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string(),
      CONTENT_SETTING_ALLOW);

  int subscription_id = GetPermissionManager()->SubscribePermissionStatusChange(
      PermissionType::GEOLOCATION, url(), url(),
      base::Bind(&PermissionManagerTest::OnPermissionChange,
                 base::Unretained(this)));

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string(),
      CONTENT_SETTING_ALLOW);

  EXPECT_FALSE(callback_called());

  GetPermissionManager()->UnsubscribePermissionStatusChange(subscription_id);
}

TEST_F(PermissionManagerTest, ChangesBackAndForth) {
  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string(),
      CONTENT_SETTING_ASK);

  int subscription_id = GetPermissionManager()->SubscribePermissionStatusChange(
      PermissionType::GEOLOCATION, url(), url(),
      base::Bind(&PermissionManagerTest::OnPermissionChange,
                 base::Unretained(this)));

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string(),
      CONTENT_SETTING_ALLOW);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::GRANTED, callback_result());

  Reset();

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string(),
      CONTENT_SETTING_ASK);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::ASK, callback_result());

  GetPermissionManager()->UnsubscribePermissionStatusChange(subscription_id);
}

TEST_F(PermissionManagerTest, SubscribeMIDIPermission) {
  int subscription_id = GetPermissionManager()->SubscribePermissionStatusChange(
      PermissionType::MIDI, url(), url(),
      base::Bind(&PermissionManagerTest::OnPermissionChange,
                 base::Unretained(this)));

  CheckPermissionStatus(PermissionType::GEOLOCATION, PermissionStatus::ASK);
  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string(),
      CONTENT_SETTING_ALLOW);
  CheckPermissionStatus(PermissionType::GEOLOCATION, PermissionStatus::GRANTED);

  EXPECT_FALSE(callback_called());

  GetPermissionManager()->UnsubscribePermissionStatusChange(subscription_id);
}
