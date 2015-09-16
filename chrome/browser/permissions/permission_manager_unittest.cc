// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_manager.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/permission_type.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::PermissionType;
using content::PermissionStatus;

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
        callback_result_(content::PERMISSION_STATUS_ASK) {
  }

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
    HostContentSettingsMapFactory::GetForProfile(&profile_)->SetContentSetting(
        ContentSettingsPattern::FromURLNoWildcard(url_),
        ContentSettingsPattern::FromURLNoWildcard(url_),
        type, std::string(), value);
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
    callback_result_ = content::PERMISSION_STATUS_ASK;
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
  CheckPermissionStatus(PermissionType::MIDI_SYSEX,
                        content::PERMISSION_STATUS_ASK);
  CheckPermissionStatus(PermissionType::PUSH_MESSAGING,
                        content::PERMISSION_STATUS_ASK);
  CheckPermissionStatus(PermissionType::NOTIFICATIONS,
                        content::PERMISSION_STATUS_ASK);
  CheckPermissionStatus(PermissionType::GEOLOCATION,
                        content::PERMISSION_STATUS_ASK);
#if defined(OS_ANDROID)
  CheckPermissionStatus(PermissionType::PROTECTED_MEDIA_IDENTIFIER,
                        content::PERMISSION_STATUS_ASK);
#endif
}

TEST_F(PermissionManagerTest, GetPermissionStatusAfterSet) {
  SetPermission(CONTENT_SETTINGS_TYPE_GEOLOCATION, CONTENT_SETTING_ALLOW);
  CheckPermissionStatus(PermissionType::GEOLOCATION,
                        content::PERMISSION_STATUS_GRANTED);

  SetPermission(CONTENT_SETTINGS_TYPE_NOTIFICATIONS, CONTENT_SETTING_ALLOW);
  CheckPermissionStatus(PermissionType::NOTIFICATIONS,
                        content::PERMISSION_STATUS_GRANTED);

  SetPermission(CONTENT_SETTINGS_TYPE_MIDI_SYSEX, CONTENT_SETTING_ALLOW);
  CheckPermissionStatus(PermissionType::MIDI_SYSEX,
                        content::PERMISSION_STATUS_GRANTED);

  SetPermission(CONTENT_SETTINGS_TYPE_PUSH_MESSAGING, CONTENT_SETTING_ALLOW);
  CheckPermissionStatus(PermissionType::PUSH_MESSAGING,
                        content::PERMISSION_STATUS_GRANTED);

#if defined(OS_ANDROID)
  SetPermission(CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER,
                CONTENT_SETTING_ALLOW);
  CheckPermissionStatus(PermissionType::PROTECTED_MEDIA_IDENTIFIER,
                        content::PERMISSION_STATUS_GRANTED);
#endif
}

TEST_F(PermissionManagerTest, SameTypeChangeNotifies) {
  int subscription_id = GetPermissionManager()->SubscribePermissionStatusChange(
      PermissionType::GEOLOCATION, url(), url(),
      base::Bind(&PermissionManagerTest::OnPermissionChange,
                 base::Unretained(this)));

  GetHostContentSettingsMap()->SetContentSetting(
      ContentSettingsPattern::FromURLNoWildcard(url()),
      ContentSettingsPattern::FromURLNoWildcard(url()),
      CONTENT_SETTINGS_TYPE_GEOLOCATION,
      std::string(),
      CONTENT_SETTING_ALLOW);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(content::PERMISSION_STATUS_GRANTED, callback_result());

  GetPermissionManager()->UnsubscribePermissionStatusChange(subscription_id);
}

TEST_F(PermissionManagerTest, DifferentTypeChangeDoesNotNotify) {
  int subscription_id = GetPermissionManager()->SubscribePermissionStatusChange(
      PermissionType::GEOLOCATION, url(), url(),
      base::Bind(&PermissionManagerTest::OnPermissionChange,
                 base::Unretained(this)));

  GetHostContentSettingsMap()->SetContentSetting(
      ContentSettingsPattern::FromURLNoWildcard(url()),
      ContentSettingsPattern::FromURLNoWildcard(url()),
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
      std::string(),
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

  GetHostContentSettingsMap()->SetContentSetting(
      ContentSettingsPattern::FromURLNoWildcard(url()),
      ContentSettingsPattern::FromURLNoWildcard(url()),
      CONTENT_SETTINGS_TYPE_GEOLOCATION,
      std::string(),
      CONTENT_SETTING_ALLOW);

  EXPECT_FALSE(callback_called());
}

TEST_F(PermissionManagerTest, DifferentPrimaryPatternDoesNotNotify) {
  int subscription_id = GetPermissionManager()->SubscribePermissionStatusChange(
      PermissionType::GEOLOCATION, url(), url(),
      base::Bind(&PermissionManagerTest::OnPermissionChange,
                 base::Unretained(this)));

  GetHostContentSettingsMap()->SetContentSetting(
      ContentSettingsPattern::FromURLNoWildcard(other_url()),
      ContentSettingsPattern::FromURLNoWildcard(url()),
      CONTENT_SETTINGS_TYPE_GEOLOCATION,
      std::string(),
      CONTENT_SETTING_ALLOW);

  EXPECT_FALSE(callback_called());

  GetPermissionManager()->UnsubscribePermissionStatusChange(subscription_id);
}

TEST_F(PermissionManagerTest, DifferentSecondaryPatternDoesNotNotify) {
  int subscription_id = GetPermissionManager()->SubscribePermissionStatusChange(
      PermissionType::GEOLOCATION, url(), url(),
      base::Bind(&PermissionManagerTest::OnPermissionChange,
                 base::Unretained(this)));

  GetHostContentSettingsMap()->SetContentSetting(
      ContentSettingsPattern::FromURLNoWildcard(url()),
      ContentSettingsPattern::FromURLNoWildcard(other_url()),
      CONTENT_SETTINGS_TYPE_GEOLOCATION,
      std::string(),
      CONTENT_SETTING_ALLOW);

  EXPECT_FALSE(callback_called());

  GetPermissionManager()->UnsubscribePermissionStatusChange(subscription_id);
}

TEST_F(PermissionManagerTest, WildCardPatternNotifies) {
  int subscription_id = GetPermissionManager()->SubscribePermissionStatusChange(
      PermissionType::GEOLOCATION, url(), url(),
      base::Bind(&PermissionManagerTest::OnPermissionChange,
                 base::Unretained(this)));

  GetHostContentSettingsMap()->SetContentSetting(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_GEOLOCATION,
      std::string(),
      CONTENT_SETTING_ALLOW);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(content::PERMISSION_STATUS_GRANTED, callback_result());

  GetPermissionManager()->UnsubscribePermissionStatusChange(subscription_id);
}

TEST_F(PermissionManagerTest, ClearSettingsNotifies) {
  GetHostContentSettingsMap()->SetContentSetting(
      ContentSettingsPattern::FromURLNoWildcard(url()),
      ContentSettingsPattern::FromURLNoWildcard(url()),
      CONTENT_SETTINGS_TYPE_GEOLOCATION,
      std::string(),
      CONTENT_SETTING_ALLOW);

  int subscription_id = GetPermissionManager()->SubscribePermissionStatusChange(
      PermissionType::GEOLOCATION, url(), url(),
      base::Bind(&PermissionManagerTest::OnPermissionChange,
                 base::Unretained(this)));

  GetHostContentSettingsMap()->ClearSettingsForOneType(
      CONTENT_SETTINGS_TYPE_GEOLOCATION);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(content::PERMISSION_STATUS_ASK, callback_result());

  GetPermissionManager()->UnsubscribePermissionStatusChange(subscription_id);
}

TEST_F(PermissionManagerTest, NewValueCorrectlyPassed) {
  int subscription_id = GetPermissionManager()->SubscribePermissionStatusChange(
      PermissionType::GEOLOCATION, url(), url(),
      base::Bind(&PermissionManagerTest::OnPermissionChange,
                 base::Unretained(this)));

  GetHostContentSettingsMap()->SetContentSetting(
      ContentSettingsPattern::FromURLNoWildcard(url()),
      ContentSettingsPattern::FromURLNoWildcard(url()),
      CONTENT_SETTINGS_TYPE_GEOLOCATION,
      std::string(),
      CONTENT_SETTING_BLOCK);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(content::PERMISSION_STATUS_DENIED, callback_result());

  GetPermissionManager()->UnsubscribePermissionStatusChange(subscription_id);
}

TEST_F(PermissionManagerTest, ChangeWithoutPermissionChangeDoesNotNotify) {
  GetHostContentSettingsMap()->SetContentSetting(
      ContentSettingsPattern::FromURLNoWildcard(url()),
      ContentSettingsPattern::FromURLNoWildcard(url()),
      CONTENT_SETTINGS_TYPE_GEOLOCATION,
      std::string(),
      CONTENT_SETTING_ALLOW);

  int subscription_id = GetPermissionManager()->SubscribePermissionStatusChange(
      PermissionType::GEOLOCATION, url(), url(),
      base::Bind(&PermissionManagerTest::OnPermissionChange,
                 base::Unretained(this)));

  GetHostContentSettingsMap()->SetContentSetting(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_GEOLOCATION,
      std::string(),
      CONTENT_SETTING_ALLOW);

  EXPECT_FALSE(callback_called());

  GetPermissionManager()->UnsubscribePermissionStatusChange(subscription_id);
}

TEST_F(PermissionManagerTest, ChangesBackAndForth) {
  GetHostContentSettingsMap()->SetContentSetting(
      ContentSettingsPattern::FromURLNoWildcard(url()),
      ContentSettingsPattern::FromURLNoWildcard(url()),
      CONTENT_SETTINGS_TYPE_GEOLOCATION,
      std::string(),
      CONTENT_SETTING_ASK);

  int subscription_id = GetPermissionManager()->SubscribePermissionStatusChange(
      PermissionType::GEOLOCATION, url(), url(),
      base::Bind(&PermissionManagerTest::OnPermissionChange,
                 base::Unretained(this)));

  GetHostContentSettingsMap()->SetContentSetting(
      ContentSettingsPattern::FromURLNoWildcard(url()),
      ContentSettingsPattern::FromURLNoWildcard(url()),
      CONTENT_SETTINGS_TYPE_GEOLOCATION,
      std::string(),
      CONTENT_SETTING_ALLOW);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(content::PERMISSION_STATUS_GRANTED, callback_result());

  Reset();

  GetHostContentSettingsMap()->SetContentSetting(
      ContentSettingsPattern::FromURLNoWildcard(url()),
      ContentSettingsPattern::FromURLNoWildcard(url()),
      CONTENT_SETTINGS_TYPE_GEOLOCATION,
      std::string(),
      CONTENT_SETTING_ASK);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(content::PERMISSION_STATUS_ASK, callback_result());

  GetPermissionManager()->UnsubscribePermissionStatusChange(subscription_id);
}
