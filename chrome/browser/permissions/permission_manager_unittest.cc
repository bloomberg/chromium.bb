// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_manager.h"

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/permissions/permission_request_manager.h"
#include "chrome/browser/permissions/permission_result.h"
#include "chrome/browser/ui/permission_bubble/mock_permission_prompt_factory.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/permission_type.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "device/vr/features/features.h"
#include "testing/gtest/include/gtest/gtest.h"

using blink::mojom::PermissionStatus;
using content::PermissionType;

namespace {

int kNoPendingOperation = -1;

class PermissionManagerTestingProfile final : public TestingProfile {
 public:
  PermissionManagerTestingProfile() {}
  ~PermissionManagerTestingProfile() override {}

  PermissionManager* GetPermissionManager() override {
    return PermissionManagerFactory::GetForProfile(this);
  }

  DISALLOW_COPY_AND_ASSIGN(PermissionManagerTestingProfile);
};

}  // namespace

class PermissionManagerTest : public ChromeRenderViewHostTestHarness {
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
    return profile_->GetPermissionManager();
  }

  HostContentSettingsMap* GetHostContentSettingsMap() {
    return HostContentSettingsMapFactory::GetForProfile(profile_.get());
  }

  void CheckPermissionStatus(PermissionType type,
                             PermissionStatus expected) {
    EXPECT_EQ(expected, GetPermissionManager()->GetPermissionStatus(
                            type, url_.GetOrigin(), url_.GetOrigin()));
  }

  void CheckPermissionResult(ContentSettingsType type,
                             ContentSetting expected_status,
                             PermissionStatusSource expected_status_source) {
    PermissionResult result = GetPermissionManager()->GetPermissionStatus(
        type, url_.GetOrigin(), url_.GetOrigin());
    EXPECT_EQ(expected_status, result.content_setting);
    EXPECT_EQ(expected_status_source, result.source);
  }

  void SetPermission(ContentSettingsType type, ContentSetting value) {
    HostContentSettingsMapFactory::GetForProfile(profile_.get())
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

  bool PendingRequestsEmpty() {
    return GetPermissionManager()->pending_requests_.IsEmpty();
  }

 private:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    profile_.reset(new PermissionManagerTestingProfile());
  }

  void TearDown() override {
    profile_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  const GURL url_;
  const GURL other_url_;
  bool callback_called_;
  PermissionStatus callback_result_;
  std::unique_ptr<PermissionManagerTestingProfile> profile_;
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
  CheckPermissionStatus(PermissionType::PUSH_MESSAGING,
                        PermissionStatus::GRANTED);

  SetPermission(CONTENT_SETTINGS_TYPE_MIDI_SYSEX, CONTENT_SETTING_ALLOW);
  CheckPermissionStatus(PermissionType::MIDI_SYSEX, PermissionStatus::GRANTED);

#if defined(OS_ANDROID)
  SetPermission(CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER,
                CONTENT_SETTING_ALLOW);
  CheckPermissionStatus(PermissionType::PROTECTED_MEDIA_IDENTIFIER,
                        PermissionStatus::GRANTED);
#endif
}

TEST_F(PermissionManagerTest, CheckPermissionResultDefault) {
  CheckPermissionResult(CONTENT_SETTINGS_TYPE_MIDI_SYSEX, CONTENT_SETTING_ASK,
                        PermissionStatusSource::UNSPECIFIED);
  CheckPermissionResult(CONTENT_SETTINGS_TYPE_PUSH_MESSAGING,
                        CONTENT_SETTING_ASK,
                        PermissionStatusSource::UNSPECIFIED);
  CheckPermissionResult(CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                        CONTENT_SETTING_ASK,
                        PermissionStatusSource::UNSPECIFIED);
  CheckPermissionResult(CONTENT_SETTINGS_TYPE_GEOLOCATION, CONTENT_SETTING_ASK,
                        PermissionStatusSource::UNSPECIFIED);
#if defined(OS_ANDROID)
  CheckPermissionResult(CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER,
                        CONTENT_SETTING_ASK,
                        PermissionStatusSource::UNSPECIFIED);
#endif
}

TEST_F(PermissionManagerTest, CheckPermissionResultAfterSet) {
  SetPermission(CONTENT_SETTINGS_TYPE_GEOLOCATION, CONTENT_SETTING_ALLOW);
  CheckPermissionResult(CONTENT_SETTINGS_TYPE_GEOLOCATION,
                        CONTENT_SETTING_ALLOW,
                        PermissionStatusSource::UNSPECIFIED);

  SetPermission(CONTENT_SETTINGS_TYPE_NOTIFICATIONS, CONTENT_SETTING_ALLOW);
  CheckPermissionResult(CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                        CONTENT_SETTING_ALLOW,
                        PermissionStatusSource::UNSPECIFIED);
  CheckPermissionResult(CONTENT_SETTINGS_TYPE_PUSH_MESSAGING,
                        CONTENT_SETTING_ALLOW,
                        PermissionStatusSource::UNSPECIFIED);

  SetPermission(CONTENT_SETTINGS_TYPE_MIDI_SYSEX, CONTENT_SETTING_ALLOW);
  CheckPermissionResult(CONTENT_SETTINGS_TYPE_MIDI_SYSEX, CONTENT_SETTING_ALLOW,
                        PermissionStatusSource::UNSPECIFIED);

#if defined(OS_ANDROID)
  SetPermission(CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER,
                CONTENT_SETTING_ALLOW);
  CheckPermissionResult(CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER,
                        CONTENT_SETTING_ALLOW,
                        PermissionStatusSource::UNSPECIFIED);
#endif
}

TEST_F(PermissionManagerTest, SubscriptionDestroyedCleanlyWithoutUnsubscribe) {
  // Test that the PermissionManager shuts down cleanly with subscriptions that
  // haven't been removed, crbug.com/720071.
  GetPermissionManager()->SubscribePermissionStatusChange(
      PermissionType::GEOLOCATION, url(), url(),
      base::Bind(&PermissionManagerTest::OnPermissionChange,
                 base::Unretained(this)));
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

TEST_F(PermissionManagerTest, SuppressPermissionRequests) {
  content::WebContents* contents = web_contents();
  vr::VrTabHelper::CreateForWebContents(contents);
  NavigateAndCommit(url());

  SetPermission(CONTENT_SETTINGS_TYPE_NOTIFICATIONS, CONTENT_SETTING_ALLOW);
  GetPermissionManager()->RequestPermission(
      PermissionType::NOTIFICATIONS, main_rfh(), url(), true,
      base::Bind(&PermissionManagerTest::OnPermissionChange,
                 base::Unretained(this)));
  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::GRANTED, callback_result());

  vr::VrTabHelper* vr_tab_helper = vr::VrTabHelper::FromWebContents(contents);
  vr_tab_helper->SetIsInVr(true);
  EXPECT_EQ(
      kNoPendingOperation,
      GetPermissionManager()->RequestPermission(
          PermissionType::NOTIFICATIONS, contents->GetMainFrame(), url(), false,
          base::Bind(&PermissionManagerTest::OnPermissionChange,
                     base::Unretained(this))));
  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::DENIED, callback_result());

  vr_tab_helper->SetIsInVr(false);
  GetPermissionManager()->RequestPermission(
      PermissionType::NOTIFICATIONS, main_rfh(), url(), false,
      base::Bind(&PermissionManagerTest::OnPermissionChange,
                 base::Unretained(this)));
  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::GRANTED, callback_result());
}

TEST_F(PermissionManagerTest, PermissionIgnoredCleanup) {
  content::WebContents* contents = web_contents();
  PermissionRequestManager::CreateForWebContents(contents);
  PermissionRequestManager* manager =
      PermissionRequestManager::FromWebContents(contents);
  auto prompt_factory = base::MakeUnique<MockPermissionPromptFactory>(manager);
  manager->DisplayPendingRequests();

  NavigateAndCommit(url());

  GetPermissionManager()->RequestPermission(
      PermissionType::VIDEO_CAPTURE, main_rfh(), url(), /*user_gesture=*/true,
      base::Bind(&PermissionManagerTest::OnPermissionChange,
                 base::Unretained(this)));

  EXPECT_FALSE(PendingRequestsEmpty());

  NavigateAndCommit(GURL("https://foobar.com"));

  EXPECT_FALSE(callback_called());
  EXPECT_TRUE(PendingRequestsEmpty());
}

// Check PermissionResult shows requests denied due to insecure origins.
TEST_F(PermissionManagerTest, InsecureOrigin) {
  GURL insecure_frame("http://www.example.com/geolocation");
  NavigateAndCommit(insecure_frame);

  PermissionResult result = GetPermissionManager()->GetPermissionStatusForFrame(
      CONTENT_SETTINGS_TYPE_GEOLOCATION, web_contents()->GetMainFrame(),
      insecure_frame);

  EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::INSECURE_ORIGIN, result.source);

  GURL secure_frame("https://www.example.com/geolocation");
  NavigateAndCommit(secure_frame);

  result = GetPermissionManager()->GetPermissionStatusForFrame(
      CONTENT_SETTINGS_TYPE_GEOLOCATION, web_contents()->GetMainFrame(),
      secure_frame);

  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);
}
