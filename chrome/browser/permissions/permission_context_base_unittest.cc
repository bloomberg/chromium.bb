// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_context_base.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/test/mock_entropy_provider.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/permissions/permission_queue_controller.h"
#include "chrome/browser/permissions/permission_request_id.h"
#include "chrome/browser/permissions/permission_util.h"
#include "chrome/browser/ui/website_settings/permission_bubble_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "chrome/browser/permissions/permission_queue_controller.h"
#else
#include "chrome/browser/ui/website_settings/permission_bubble_manager.h"
#endif

const char* kPermissionsKillSwitchFieldStudy =
    PermissionContextBase::kPermissionsKillSwitchFieldStudy;
const char* kPermissionsKillSwitchBlockedValue =
    PermissionContextBase::kPermissionsKillSwitchBlockedValue;
const char kPermissionsKillSwitchTestGroup[] = "TestGroup";

class TestPermissionContext : public PermissionContextBase {
 public:
  TestPermissionContext(Profile* profile,
                  const ContentSettingsType permission_type)
   : PermissionContextBase(profile, permission_type),
     permission_set_(false),
     permission_granted_(false),
     tab_context_updated_(false),
     field_trial_list_(new base::FieldTrialList(new base::MockEntropyProvider))
     {}

  ~TestPermissionContext() override {}

#if defined(OS_ANDROID)
  PermissionQueueController* GetInfoBarController() {
    return GetQueueController();
  }
#endif

  bool permission_granted() {
    return permission_granted_;
  }

  bool permission_set() {
    return permission_set_;
  }

  bool tab_context_updated() {
    return tab_context_updated_;
  }

  void TrackPermissionDecision(ContentSetting content_setting) {
    permission_set_ = true;
    permission_granted_ = content_setting == CONTENT_SETTING_ALLOW;
  }

  void ResetFieldTrialList() {
    // Destroy the existing FieldTrialList before creating a new one to avoid
    // a DCHECK.
    field_trial_list_.reset();
    field_trial_list_.reset(new base::FieldTrialList(
        new base::MockEntropyProvider));
    variations::testing::ClearAllVariationParams();
  }

 protected:
  void UpdateTabContext(const PermissionRequestID& id,
                        const GURL& requesting_origin,
                        bool allowed) override {
    tab_context_updated_ = true;
  }

  bool IsRestrictedToSecureOrigins() const override {
    return false;
  }

 private:
   bool permission_set_;
   bool permission_granted_;
   bool tab_context_updated_;
   scoped_ptr<base::FieldTrialList> field_trial_list_;
};

class PermissionContextBaseTests : public ChromeRenderViewHostTestHarness {
 protected:
  PermissionContextBaseTests() {}

  // Accept or dismiss the permission bubble or infobar.
  void RespondToPermission(TestPermissionContext* context,
                           const PermissionRequestID& id,
                           const GURL& url,
                           bool accept) {
#if defined(OS_ANDROID)
    context->GetInfoBarController()->OnPermissionSet(id, url, url, accept,
                                                     accept);
#else
    PermissionBubbleManager* manager =
        PermissionBubbleManager::FromWebContents(web_contents());
    if (accept)
      manager->Accept();
    else
      manager->Closing();
#endif
  }

  void TestAskAndGrant_TestContent() {
    TestPermissionContext permission_context(
        profile(), CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
    GURL url("http://www.google.com");
    content::WebContentsTester::For(web_contents())->NavigateAndCommit(url);

    const PermissionRequestID id(
        web_contents()->GetRenderProcessHost()->GetID(),
        web_contents()->GetMainFrame()->GetRoutingID(),
        -1);
    permission_context.RequestPermission(
        web_contents(),
        id, url, true,
        base::Bind(&TestPermissionContext::TrackPermissionDecision,
                   base::Unretained(&permission_context)));

    RespondToPermission(&permission_context, id, url, true);
    EXPECT_TRUE(permission_context.permission_set());
    EXPECT_TRUE(permission_context.permission_granted());
    EXPECT_TRUE(permission_context.tab_context_updated());

    ContentSetting setting =
        HostContentSettingsMapFactory::GetForProfile(profile())
            ->GetContentSetting(url.GetOrigin(),
                                url.GetOrigin(),
                                CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                                std::string());
    EXPECT_EQ(CONTENT_SETTING_ALLOW, setting);
  }

  void TestAskAndDismiss_TestContent() {
    TestPermissionContext permission_context(
        profile(), CONTENT_SETTINGS_TYPE_MIDI_SYSEX);
    GURL url("http://www.google.es");
    content::WebContentsTester::For(web_contents())->NavigateAndCommit(url);

    const PermissionRequestID id(
        web_contents()->GetRenderProcessHost()->GetID(),
        web_contents()->GetMainFrame()->GetRoutingID(),
        -1);
    permission_context.RequestPermission(
        web_contents(),
        id, url, true,
        base::Bind(&TestPermissionContext::TrackPermissionDecision,
                   base::Unretained(&permission_context)));

    RespondToPermission(&permission_context, id, url, false);
    EXPECT_TRUE(permission_context.permission_set());
    EXPECT_FALSE(permission_context.permission_granted());
    EXPECT_TRUE(permission_context.tab_context_updated());

    ContentSetting setting =
        HostContentSettingsMapFactory::GetForProfile(profile())
            ->GetContentSetting(url.GetOrigin(),
                                url.GetOrigin(),
                                CONTENT_SETTINGS_TYPE_MIDI_SYSEX,
                                std::string());
    EXPECT_EQ(CONTENT_SETTING_ASK, setting);
  }

  void TestRequestPermissionInvalidUrl(ContentSettingsType type) {
    TestPermissionContext permission_context(profile(), type);
    GURL url;
    ASSERT_FALSE(url.is_valid());
    content::WebContentsTester::For(web_contents())->NavigateAndCommit(url);

    const PermissionRequestID id(
        web_contents()->GetRenderProcessHost()->GetID(),
        web_contents()->GetMainFrame()->GetRoutingID(),
        -1);
    permission_context.RequestPermission(
        web_contents(),
        id, url, true,
        base::Bind(&TestPermissionContext::TrackPermissionDecision,
                   base::Unretained(&permission_context)));

    EXPECT_TRUE(permission_context.permission_set());
    EXPECT_FALSE(permission_context.permission_granted());
    EXPECT_TRUE(permission_context.tab_context_updated());

    ContentSetting setting =
        HostContentSettingsMapFactory::GetForProfile(profile())
            ->GetContentSetting(url.GetOrigin(),
                                url.GetOrigin(),
                                type,
                                std::string());
    EXPECT_EQ(CONTENT_SETTING_ASK, setting);
  }

  void TestGrantAndRevoke_TestContent(ContentSettingsType type,
                                      ContentSetting expected_default) {
    TestPermissionContext permission_context(profile(), type);
    GURL url("https://www.google.com");
    content::WebContentsTester::For(web_contents())->NavigateAndCommit(url);

    const PermissionRequestID id(
        web_contents()->GetRenderProcessHost()->GetID(),
        web_contents()->GetMainFrame()->GetRoutingID(),
        -1);
    permission_context.RequestPermission(
        web_contents(),
        id, url, true,
        base::Bind(&TestPermissionContext::TrackPermissionDecision,
                   base::Unretained(&permission_context)));

    RespondToPermission(&permission_context, id, url, true);
    EXPECT_TRUE(permission_context.permission_set());
    EXPECT_TRUE(permission_context.permission_granted());
    EXPECT_TRUE(permission_context.tab_context_updated());

    ContentSetting setting =
        HostContentSettingsMapFactory::GetForProfile(profile())
            ->GetContentSetting(url.GetOrigin(),
                                url.GetOrigin(),
                                type,
                                std::string());
    EXPECT_EQ(CONTENT_SETTING_ALLOW, setting);

    // Try to reset permission.
    permission_context.ResetPermission(url.GetOrigin(), url.GetOrigin());
    ContentSetting setting_after_reset =
        HostContentSettingsMapFactory::GetForProfile(profile())
            ->GetContentSetting(url.GetOrigin(),
                                url.GetOrigin(),
                                type,
                                std::string());
    ContentSetting default_setting =
        HostContentSettingsMapFactory::GetForProfile(profile())
            ->GetDefaultContentSetting(type, nullptr);
    EXPECT_EQ(default_setting, setting_after_reset);
  }

  void TestGlobalPermissionsKillSwitch(ContentSettingsType type) {
    TestPermissionContext permission_context(profile(), type);
    permission_context.ResetFieldTrialList();

    EXPECT_FALSE(permission_context.IsPermissionKillSwitchOn());
    std::map<std::string, std::string> params;
    params[PermissionUtil::GetPermissionString(type)] =
        kPermissionsKillSwitchBlockedValue;
    variations::AssociateVariationParams(
        kPermissionsKillSwitchFieldStudy, kPermissionsKillSwitchTestGroup,
        params);
    base::FieldTrialList::CreateFieldTrial(kPermissionsKillSwitchFieldStudy,
                                           kPermissionsKillSwitchTestGroup);
    EXPECT_TRUE(permission_context.IsPermissionKillSwitchOn());
  }

 private:
  // ChromeRenderViewHostTestHarness:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
#if defined(OS_ANDROID)
    InfoBarService::CreateForWebContents(web_contents());
#else
    PermissionBubbleManager::CreateForWebContents(web_contents());
#endif
  }

  DISALLOW_COPY_AND_ASSIGN(PermissionContextBaseTests);
};

// Simulates clicking Accept. The permission should be granted and
// saved for future use.
TEST_F(PermissionContextBaseTests, TestAskAndGrant) {
  TestAskAndGrant_TestContent();
}

// Simulates clicking Dismiss (X) in the infobar/bubble.
// The permission should be denied but not saved for future use.
TEST_F(PermissionContextBaseTests, TestAskAndDismiss) {
  TestAskAndDismiss_TestContent();
}

// Simulates non-valid requesting URL.
// The permission should be denied but not saved for future use.
TEST_F(PermissionContextBaseTests, TestNonValidRequestingUrl) {
  TestRequestPermissionInvalidUrl(CONTENT_SETTINGS_TYPE_GEOLOCATION);
  TestRequestPermissionInvalidUrl(CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
  TestRequestPermissionInvalidUrl(CONTENT_SETTINGS_TYPE_MIDI_SYSEX);
  TestRequestPermissionInvalidUrl(CONTENT_SETTINGS_TYPE_PUSH_MESSAGING);
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
  TestRequestPermissionInvalidUrl(
      CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER);
#endif
}

#if defined(OS_ANDROID)
// This test is specific to Android because other platforms use bubbles.
TEST_F(PermissionContextBaseTests, TestGrantAndRevokeWithInfobars) {
  TestGrantAndRevoke_TestContent(CONTENT_SETTINGS_TYPE_GEOLOCATION,
                                 CONTENT_SETTING_ASK);
  TestGrantAndRevoke_TestContent(CONTENT_SETTINGS_TYPE_MIDI_SYSEX,
                                 CONTENT_SETTING_ASK);
  TestGrantAndRevoke_TestContent(
      CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER, CONTENT_SETTING_ASK);
  // TODO(timvolodine): currently no test for
  // CONTENT_SETTINGS_TYPE_NOTIFICATIONS because notification permissions work
  // differently with infobars as compared to bubbles (crbug.com/453784).
  // TODO(timvolodine): currently no test for
  // CONTENT_SETTINGS_TYPE_PUSH_MESSAGING because infobars do not implement push
  // messaging permissions (crbug.com/453788).
}
#endif

#if !defined(OS_ANDROID) && !defined(OS_IOS)
// Simulates granting and revoking of permissions using permission bubbles.
// This test shouldn't run on mobile because mobile platforms use infobars.
TEST_F(PermissionContextBaseTests, TestGrantAndRevokeWithBubbles) {
  TestGrantAndRevoke_TestContent(CONTENT_SETTINGS_TYPE_GEOLOCATION,
                                 CONTENT_SETTING_ASK);
#if defined(ENABLE_NOTIFICATIONS)
  TestGrantAndRevoke_TestContent(CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                                 CONTENT_SETTING_ASK);
#endif
  TestGrantAndRevoke_TestContent(CONTENT_SETTINGS_TYPE_MIDI_SYSEX,
                                 CONTENT_SETTING_ASK);
  TestGrantAndRevoke_TestContent(CONTENT_SETTINGS_TYPE_PUSH_MESSAGING,
                                 CONTENT_SETTING_ASK);
}
#endif

// Tests the global kill switch by enabling/disabling the Field Trials.
TEST_F(PermissionContextBaseTests, TestGlobalKillSwitch) {
  TestGlobalPermissionsKillSwitch(CONTENT_SETTINGS_TYPE_GEOLOCATION);
  TestGlobalPermissionsKillSwitch(CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
  TestGlobalPermissionsKillSwitch(CONTENT_SETTINGS_TYPE_MIDI_SYSEX);
  TestGlobalPermissionsKillSwitch(CONTENT_SETTINGS_TYPE_PUSH_MESSAGING);
  TestGlobalPermissionsKillSwitch(CONTENT_SETTINGS_TYPE_DURABLE_STORAGE);
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
  TestGlobalPermissionsKillSwitch(
      CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER);
#endif
  TestGlobalPermissionsKillSwitch(CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC);
  TestGlobalPermissionsKillSwitch(CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA);
}
