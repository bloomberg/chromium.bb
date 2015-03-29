// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_manager.h"

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
  PermissionManagerTest() : url_("https://example.com") {}

  void CheckPermissionStatus(PermissionType type,
                             PermissionStatus expected) {
    EXPECT_EQ(expected,
              profile_.GetPermissionManager()->GetPermissionStatus(
                  type, url_.GetOrigin(), url_.GetOrigin()));
  }

  void SetPermission(ContentSettingsType type, ContentSetting value) {
    profile_.GetHostContentSettingsMap()->SetContentSetting(
        ContentSettingsPattern::FromURLNoWildcard(url_),
        ContentSettingsPattern::FromURLNoWildcard(url_),
        type, std::string(), value);
  }

 private:
  const GURL url_;
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
