// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_context_base.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/test/histogram_tester.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker.h"
#include "chrome/browser/permissions/permission_queue_controller.h"
#include "chrome/browser/permissions/permission_request_id.h"
#include "chrome/browser/permissions/permission_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/permissions/permission_request_manager.h"
#endif

const char* const kPermissionsKillSwitchFieldStudy =
    PermissionContextBase::kPermissionsKillSwitchFieldStudy;
const char* const kPermissionsKillSwitchBlockedValue =
    PermissionContextBase::kPermissionsKillSwitchBlockedValue;
const char kPermissionsKillSwitchTestGroup[] = "TestGroup";
const char* const kPromptGroupName = kPermissionsKillSwitchTestGroup;
const char kPromptTrialName[] = "PermissionPromptsUX";

class TestPermissionContext : public PermissionContextBase {
 public:
  TestPermissionContext(Profile* profile,
                        const content::PermissionType permission_type,
                        const ContentSettingsType content_settings_type)
      : PermissionContextBase(profile, permission_type, content_settings_type),
        tab_context_updated_(false) {}

  ~TestPermissionContext() override {}

#if defined(OS_ANDROID)
  PermissionQueueController* GetInfoBarController() {
    return GetQueueController();
  }
#endif

  const std::vector<ContentSetting>& decisions() const { return decisions_; }

  bool tab_context_updated() const { return tab_context_updated_; }

  void TrackPermissionDecision(ContentSetting content_setting) {
    decisions_.push_back(content_setting);
  }

  ContentSetting GetContentSettingFromMap(const GURL& url_a,
                                          const GURL& url_b) {
    auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
    return map->GetContentSetting(url_a.GetOrigin(), url_b.GetOrigin(),
                                  content_settings_type(), std::string());
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
  std::vector<ContentSetting> decisions_;
  bool tab_context_updated_;

  DISALLOW_COPY_AND_ASSIGN(TestPermissionContext);
};

class TestKillSwitchPermissionContext : public TestPermissionContext {
 public:
  TestKillSwitchPermissionContext(
      Profile* profile,
      const content::PermissionType permission_type,
      const ContentSettingsType content_settings_type)
      : TestPermissionContext(profile, permission_type, content_settings_type),
        field_trial_list_(base::MakeUnique<base::FieldTrialList>(
            base::MakeUnique<base::MockEntropyProvider>())) {}

  void ResetFieldTrialList() {
    // Destroy the existing FieldTrialList before creating a new one to avoid
    // a DCHECK.
    field_trial_list_.reset();
    field_trial_list_ = base::MakeUnique<base::FieldTrialList>(
        base::MakeUnique<base::MockEntropyProvider>());
    variations::testing::ClearAllVariationParams();
  }

 private:
  std::unique_ptr<base::FieldTrialList> field_trial_list_;

  DISALLOW_COPY_AND_ASSIGN(TestKillSwitchPermissionContext);
};

class PermissionContextBaseTests : public ChromeRenderViewHostTestHarness {
 protected:
  PermissionContextBaseTests() {}
  ~PermissionContextBaseTests() override {}

  // Accept or dismiss the permission bubble or infobar.
  void RespondToPermission(TestPermissionContext* context,
                           const PermissionRequestID& id,
                           const GURL& url,
                           bool persist,
                           ContentSetting response) {
    DCHECK(response == CONTENT_SETTING_ALLOW ||
           response == CONTENT_SETTING_BLOCK ||
           response == CONTENT_SETTING_ASK);
#if defined(OS_ANDROID)
    PermissionAction decision = DISMISSED;
    if (response == CONTENT_SETTING_ALLOW)
      decision = GRANTED;
    else if (response == CONTENT_SETTING_BLOCK)
      decision = DENIED;
    context->GetInfoBarController()->OnPermissionSet(
        id, url, url, false /* user_gesture */, persist, decision);
#else
    PermissionRequestManager* manager =
        PermissionRequestManager::FromWebContents(web_contents());
    manager->TogglePersist(persist);
    switch (response) {
      case CONTENT_SETTING_ALLOW:
        manager->Accept();
        break;
      case CONTENT_SETTING_BLOCK:
        manager->Deny();
        break;
      case CONTENT_SETTING_ASK:
        manager->Closing();
        break;
      default:
        NOTREACHED();
    }
#endif
  }

  void TestAskAndDecide_TestContent(content::PermissionType permission,
                                    ContentSettingsType content_settings_type,
                                    ContentSetting decision,
                                    bool persist) {
    TestPermissionContext permission_context(profile(), permission,
                                             content_settings_type);
    GURL url("https://www.google.com");
    NavigateAndCommit(url);
    base::HistogramTester histograms;

    const PermissionRequestID id(
        web_contents()->GetRenderProcessHost()->GetID(),
        web_contents()->GetMainFrame()->GetRoutingID(),
        -1);
    permission_context.RequestPermission(
        web_contents(),
        id, url, true /* user_gesture */,
        base::Bind(&TestPermissionContext::TrackPermissionDecision,
                   base::Unretained(&permission_context)));

    RespondToPermission(&permission_context, id, url, persist, decision);
    ASSERT_EQ(1u, permission_context.decisions().size());
    EXPECT_EQ(decision, permission_context.decisions()[0]);
    EXPECT_TRUE(permission_context.tab_context_updated());

    std::string decision_string;
    if (decision == CONTENT_SETTING_ALLOW)
      decision_string = "Accepted";
    else if (decision == CONTENT_SETTING_BLOCK)
      decision_string = "Denied";
    else if (decision == CONTENT_SETTING_ASK)
      decision_string = "Dismissed";

    if (decision_string.size()) {
      histograms.ExpectUniqueSample(
          "Permissions.Prompt." + decision_string + ".PriorDismissCount." +
              PermissionUtil::GetPermissionString(permission),
          0, 1);
      histograms.ExpectUniqueSample(
          "Permissions.Prompt." + decision_string + ".PriorIgnoreCount." +
              PermissionUtil::GetPermissionString(permission),
          0, 1);
    }

    if (persist) {
      EXPECT_EQ(decision,
                permission_context.GetContentSettingFromMap(url, url));
    } else {
      EXPECT_EQ(CONTENT_SETTING_ASK,
                permission_context.GetContentSettingFromMap(url, url));
    }
  }

  void DismissMultipleTimesAndExpectBlock(
      const GURL& url,
      content::PermissionType permission_type,
      ContentSettingsType content_settings_type,
      uint32_t iterations) {
    base::HistogramTester histograms;

    // Dismiss |iterations| times. The final dismiss should change the decision
    // from dismiss to block, and hence change the persisted content setting.
    for (uint32_t i = 0; i < iterations; ++i) {
      TestPermissionContext permission_context(
          profile(), permission_type, content_settings_type);
      ContentSetting expected =
          (i < (iterations - 1)) ? CONTENT_SETTING_ASK : CONTENT_SETTING_BLOCK;

      const PermissionRequestID id(
          web_contents()->GetRenderProcessHost()->GetID(),
          web_contents()->GetMainFrame()->GetRoutingID(), i);
      permission_context.RequestPermission(
          web_contents(), id, url, true /* user_gesture */,
          base::Bind(&TestPermissionContext::TrackPermissionDecision,
                    base::Unretained(&permission_context)));

      RespondToPermission(&permission_context, id, url, false, /* persist */
                          CONTENT_SETTING_ASK);
      histograms.ExpectTotalCount(
          "Permissions.Prompt.Dismissed.PriorDismissCount." +
              PermissionUtil::GetPermissionString(permission_type),
          i + 1);
      histograms.ExpectBucketCount(
          "Permissions.Prompt.Dismissed.PriorDismissCount." +
              PermissionUtil::GetPermissionString(permission_type),
          i, 1);

      ASSERT_EQ(1u, permission_context.decisions().size());
      EXPECT_EQ(expected, permission_context.decisions()[0]);
      EXPECT_TRUE(permission_context.tab_context_updated());
      EXPECT_EQ(expected,
                permission_context.GetContentSettingFromMap(url, url));
    }

    // Ensure that we finish in the block state.
    TestPermissionContext permission_context(
        profile(), permission_type, content_settings_type);
    EXPECT_EQ(CONTENT_SETTING_BLOCK,
              permission_context.GetContentSettingFromMap(url, url));
  }

  void TestBlockOnSeveralDismissals_TestContent() {
    GURL url("https://www.google.com");
    NavigateAndCommit(url);
    base::HistogramTester histograms;

    // First, ensure that > 3 dismissals behaves correctly.
    for (uint32_t i = 0; i < 4; ++i) {
      TestPermissionContext permission_context(
          profile(), content::PermissionType::GEOLOCATION,
          CONTENT_SETTINGS_TYPE_GEOLOCATION);

      const PermissionRequestID id(
          web_contents()->GetRenderProcessHost()->GetID(),
          web_contents()->GetMainFrame()->GetRoutingID(), i);
      permission_context.RequestPermission(
          web_contents(), id, url, true /* user_gesture */,
          base::Bind(&TestPermissionContext::TrackPermissionDecision,
                    base::Unretained(&permission_context)));

      RespondToPermission(&permission_context, id, url, false, /* persist */
                          CONTENT_SETTING_ASK);
      histograms.ExpectTotalCount(
          "Permissions.Prompt.Dismissed.PriorDismissCount.Geolocation",
          i + 1);
      histograms.ExpectBucketCount(
          "Permissions.Prompt.Dismissed.PriorDismissCount.Geolocation", i, 1);
      ASSERT_EQ(1u, permission_context.decisions().size());
      EXPECT_EQ(CONTENT_SETTING_ASK, permission_context.decisions()[0]);
      EXPECT_TRUE(permission_context.tab_context_updated());
      EXPECT_EQ(CONTENT_SETTING_ASK,
                permission_context.GetContentSettingFromMap(url, url));
    }

    // Flush the dismissal counts. Enable the block on too many dismissals
    // feature, which is disabled by default.
    auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
    map->ClearSettingsForOneType(
        CONTENT_SETTINGS_TYPE_PROMPT_NO_DECISION_COUNT);

    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(features::kBlockPromptsIfDismissedOften);

    EXPECT_TRUE(
        base::FeatureList::IsEnabled(features::kBlockPromptsIfDismissedOften));

    // Sanity check independence per permission type by checking two of them.
    DismissMultipleTimesAndExpectBlock(url,
                                       content::PermissionType::GEOLOCATION,
                                       CONTENT_SETTINGS_TYPE_GEOLOCATION, 3);
    DismissMultipleTimesAndExpectBlock(url,
                                       content::PermissionType::NOTIFICATIONS,
                                       CONTENT_SETTINGS_TYPE_NOTIFICATIONS, 3);
  }

  void TestVariationBlockOnSeveralDismissals_TestContent() {
    GURL url("https://www.google.com");
    NavigateAndCommit(url);
    base::HistogramTester histograms;

    // Set up the custom parameter and custom value.
    base::FieldTrialList field_trials(nullptr);
    base::FieldTrial* trial = base::FieldTrialList::CreateFieldTrial(
        kPromptTrialName, kPromptGroupName);
    std::map<std::string, std::string> params;
    params[PermissionDecisionAutoBlocker::kPromptDismissCountKey] = "5";
    ASSERT_TRUE(variations::AssociateVariationParams(
        kPromptTrialName, kPromptGroupName, params));

    std::unique_ptr<base::FeatureList> feature_list =
        base::MakeUnique<base::FeatureList>();
    feature_list->RegisterFieldTrialOverride(
        features::kBlockPromptsIfDismissedOften.name,
        base::FeatureList::OVERRIDE_ENABLE_FEATURE, trial);

    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatureList(std::move(feature_list));

    EXPECT_EQ(base::FeatureList::GetFieldTrial(
                  features::kBlockPromptsIfDismissedOften),
              trial);

    {
      std::map<std::string, std::string> actual_params;
      EXPECT_TRUE(variations::GetVariationParamsByFeature(
          features::kBlockPromptsIfDismissedOften, &actual_params));
      EXPECT_EQ(params, actual_params);
    }

    for (uint32_t i = 0; i < 5; ++i) {
      TestPermissionContext permission_context(
          profile(), content::PermissionType::MIDI_SYSEX,
          CONTENT_SETTINGS_TYPE_MIDI_SYSEX);

      ContentSetting expected =
          (i < 4) ? CONTENT_SETTING_ASK : CONTENT_SETTING_BLOCK;
      const PermissionRequestID id(
          web_contents()->GetRenderProcessHost()->GetID(),
          web_contents()->GetMainFrame()->GetRoutingID(), i);
      permission_context.RequestPermission(
          web_contents(), id, url, true /* user_gesture */,
          base::Bind(&TestPermissionContext::TrackPermissionDecision,
                    base::Unretained(&permission_context)));

      RespondToPermission(&permission_context, id, url, false, /* persist */
                          CONTENT_SETTING_ASK);
      EXPECT_EQ(1u, permission_context.decisions().size());
      ASSERT_EQ(expected, permission_context.decisions()[0]);
      EXPECT_TRUE(permission_context.tab_context_updated());
      EXPECT_EQ(expected,
                permission_context.GetContentSettingFromMap(url, url));

      histograms.ExpectTotalCount(
          "Permissions.Prompt.Dismissed.PriorDismissCount.MidiSysEx", i + 1);
      histograms.ExpectBucketCount(
          "Permissions.Prompt.Dismissed.PriorDismissCount.MidiSysEx", i, 1);
    }

    // Ensure that we finish in the block state.
    TestPermissionContext permission_context(
        profile(), content::PermissionType::MIDI_SYSEX,
        CONTENT_SETTINGS_TYPE_MIDI_SYSEX);
    EXPECT_EQ(CONTENT_SETTING_BLOCK,
              permission_context.GetContentSettingFromMap(url, url));
    variations::testing::ClearAllVariationParams();
  }

  void TestRequestPermissionInvalidUrl(
      content::PermissionType permission_type,
      ContentSettingsType content_settings_type) {
    TestPermissionContext permission_context(profile(), permission_type,
                                             content_settings_type);
    GURL url;
    ASSERT_FALSE(url.is_valid());
    NavigateAndCommit(url);

    const PermissionRequestID id(
        web_contents()->GetRenderProcessHost()->GetID(),
        web_contents()->GetMainFrame()->GetRoutingID(),
        -1);
    permission_context.RequestPermission(
        web_contents(),
        id, url, true /* user_gesture */,
        base::Bind(&TestPermissionContext::TrackPermissionDecision,
                   base::Unretained(&permission_context)));

    ASSERT_EQ(1u, permission_context.decisions().size());
    EXPECT_EQ(CONTENT_SETTING_BLOCK, permission_context.decisions()[0]);
    EXPECT_TRUE(permission_context.tab_context_updated());
    EXPECT_EQ(CONTENT_SETTING_ASK,
              permission_context.GetContentSettingFromMap(url, url));
  }

  void TestGrantAndRevoke_TestContent(content::PermissionType permission_type,
                                      ContentSettingsType content_settings_type,
                                      ContentSetting expected_default) {
    TestPermissionContext permission_context(profile(), permission_type,
                                             content_settings_type);
    GURL url("https://www.google.com");
    NavigateAndCommit(url);

    const PermissionRequestID id(
        web_contents()->GetRenderProcessHost()->GetID(),
        web_contents()->GetMainFrame()->GetRoutingID(),
        -1);
    permission_context.RequestPermission(
        web_contents(),
        id, url, true /* user_gesture */,
        base::Bind(&TestPermissionContext::TrackPermissionDecision,
                   base::Unretained(&permission_context)));

    RespondToPermission(&permission_context, id, url, true, /* persist */
                        CONTENT_SETTING_ALLOW);
    ASSERT_EQ(1u, permission_context.decisions().size());
    EXPECT_EQ(CONTENT_SETTING_ALLOW, permission_context.decisions()[0]);
    EXPECT_TRUE(permission_context.tab_context_updated());
    EXPECT_EQ(CONTENT_SETTING_ALLOW,
              permission_context.GetContentSettingFromMap(url, url));

    // Try to reset permission.
    permission_context.ResetPermission(url.GetOrigin(), url.GetOrigin());
    ContentSetting setting_after_reset =
        permission_context.GetContentSettingFromMap(url, url);
    ContentSetting default_setting =
        HostContentSettingsMapFactory::GetForProfile(profile())
            ->GetDefaultContentSetting(content_settings_type, nullptr);
    EXPECT_EQ(default_setting, setting_after_reset);
  }

  void TestGlobalPermissionsKillSwitch(
      content::PermissionType permission_type,
      ContentSettingsType content_settings_type) {
    TestKillSwitchPermissionContext permission_context(
        profile(), permission_type, content_settings_type);
    permission_context.ResetFieldTrialList();

    EXPECT_FALSE(permission_context.IsPermissionKillSwitchOn());
    std::map<std::string, std::string> params;
    params[PermissionUtil::GetPermissionString(permission_type)] =
        kPermissionsKillSwitchBlockedValue;
    variations::AssociateVariationParams(
        kPermissionsKillSwitchFieldStudy, kPermissionsKillSwitchTestGroup,
        params);
    base::FieldTrialList::CreateFieldTrial(kPermissionsKillSwitchFieldStudy,
                                           kPermissionsKillSwitchTestGroup);
    EXPECT_TRUE(permission_context.IsPermissionKillSwitchOn());
  }

  // Don't call this more than once in the same test, as it persists data to
  // HostContentSettingsMap.
  void TestParallelRequests(ContentSetting response) {
    TestPermissionContext permission_context(
        profile(), content::PermissionType::NOTIFICATIONS,
        CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
    GURL url("http://www.google.com");
    NavigateAndCommit(url);

    const PermissionRequestID id0(
        web_contents()->GetRenderProcessHost()->GetID(),
        web_contents()->GetMainFrame()->GetRoutingID(), 0);
    const PermissionRequestID id1(
        web_contents()->GetRenderProcessHost()->GetID(),
        web_contents()->GetMainFrame()->GetRoutingID(), 1);

    permission_context.RequestPermission(
        web_contents(), id0, url, true /* user_gesture */,
        base::Bind(&TestPermissionContext::TrackPermissionDecision,
                   base::Unretained(&permission_context)));
    permission_context.RequestPermission(
        web_contents(), id1, url, true /* user_gesture */,
        base::Bind(&TestPermissionContext::TrackPermissionDecision,
                   base::Unretained(&permission_context)));

    EXPECT_EQ(0u, permission_context.decisions().size());

    bool persist = (response == CONTENT_SETTING_ALLOW ||
                    response == CONTENT_SETTING_BLOCK);
    RespondToPermission(&permission_context, id0, url, persist, response);

    ASSERT_EQ(2u, permission_context.decisions().size());
    EXPECT_EQ(response, permission_context.decisions()[0]);
    EXPECT_EQ(response, permission_context.decisions()[1]);
    EXPECT_TRUE(permission_context.tab_context_updated());

    EXPECT_EQ(response, permission_context.GetContentSettingFromMap(url, url));
  }

 private:
  // ChromeRenderViewHostTestHarness:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
#if defined(OS_ANDROID)
    InfoBarService::CreateForWebContents(web_contents());
#else
    PermissionRequestManager::CreateForWebContents(web_contents());
#endif
  }

  DISALLOW_COPY_AND_ASSIGN(PermissionContextBaseTests);
};

// Simulates clicking Accept. The permission should be granted and
// saved for future use.
TEST_F(PermissionContextBaseTests, TestAskAndGrantPersist) {
  TestAskAndDecide_TestContent(content::PermissionType::NOTIFICATIONS,
                               CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                               CONTENT_SETTING_ALLOW, true);
}

// Simulates clicking Accept. The permission should be granted, but not
// persisted.
TEST_F(PermissionContextBaseTests, TestAskAndGrantNoPersist) {
  TestAskAndDecide_TestContent(content::PermissionType::NOTIFICATIONS,
                               CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                               CONTENT_SETTING_ALLOW, false);
}

// Simulates clicking Block. The permission should be denied and
// saved for future use.
TEST_F(PermissionContextBaseTests, TestAskAndBlockPersist) {
  TestAskAndDecide_TestContent(content::PermissionType::GEOLOCATION,
                               CONTENT_SETTINGS_TYPE_GEOLOCATION,
                               CONTENT_SETTING_BLOCK, true);
}

// Simulates clicking Block. The permission should be denied, but not persisted.
TEST_F(PermissionContextBaseTests, TestAskAndBlockNoPersist) {
  TestAskAndDecide_TestContent(content::PermissionType::GEOLOCATION,
                               CONTENT_SETTINGS_TYPE_GEOLOCATION,
                               CONTENT_SETTING_BLOCK, false);
}

// Simulates clicking Dismiss (X) in the infobar/bubble.
// The permission should be denied but not saved for future use.
TEST_F(PermissionContextBaseTests, TestAskAndDismiss) {
  TestAskAndDecide_TestContent(content::PermissionType::MIDI_SYSEX,
                               CONTENT_SETTINGS_TYPE_MIDI_SYSEX,
                               CONTENT_SETTING_ASK, false);
}

// Simulates clicking Dismiss (X) in the infobar/bubble with the block on too
// many dismissals feature active. The permission should be blocked after
// several dismissals.
TEST_F(PermissionContextBaseTests, TestDismissUntilBlocked) {
  TestBlockOnSeveralDismissals_TestContent();
}

// Test setting a custom number of dismissals before block via variations.
TEST_F(PermissionContextBaseTests, TestDismissVariations) {
  TestVariationBlockOnSeveralDismissals_TestContent();
}

// Simulates non-valid requesting URL.
// The permission should be denied but not saved for future use.
TEST_F(PermissionContextBaseTests, TestNonValidRequestingUrl) {
  TestRequestPermissionInvalidUrl(content::PermissionType::GEOLOCATION,
                                  CONTENT_SETTINGS_TYPE_GEOLOCATION);
  TestRequestPermissionInvalidUrl(content::PermissionType::NOTIFICATIONS,
                                  CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
  TestRequestPermissionInvalidUrl(content::PermissionType::MIDI_SYSEX,
                                  CONTENT_SETTINGS_TYPE_MIDI_SYSEX);
  TestRequestPermissionInvalidUrl(content::PermissionType::PUSH_MESSAGING,
                                  CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
  TestRequestPermissionInvalidUrl(
      content::PermissionType::PROTECTED_MEDIA_IDENTIFIER,
      CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER);
#endif
}

#if defined(OS_ANDROID)
// This test is specific to Android because other platforms use bubbles.
TEST_F(PermissionContextBaseTests, TestGrantAndRevokeWithInfobars) {
  TestGrantAndRevoke_TestContent(content::PermissionType::GEOLOCATION,
                                 CONTENT_SETTINGS_TYPE_GEOLOCATION,
                                 CONTENT_SETTING_ASK);
  TestGrantAndRevoke_TestContent(content::PermissionType::MIDI_SYSEX,
                                 CONTENT_SETTINGS_TYPE_MIDI_SYSEX,
                                 CONTENT_SETTING_ASK);
  TestGrantAndRevoke_TestContent(
      content::PermissionType::PROTECTED_MEDIA_IDENTIFIER,
      CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER, CONTENT_SETTING_ASK);
  // TODO(timvolodine): currently no test for
  // CONTENT_SETTINGS_TYPE_NOTIFICATIONS because notification permissions work
  // differently with infobars as compared to bubbles (crbug.com/453784).
}
#endif

#if !defined(OS_ANDROID)
// Simulates granting and revoking of permissions using permission bubbles.
// This test shouldn't run on mobile because mobile platforms use infobars.
TEST_F(PermissionContextBaseTests, TestGrantAndRevokeWithBubbles) {
  TestGrantAndRevoke_TestContent(content::PermissionType::GEOLOCATION,
                                 CONTENT_SETTINGS_TYPE_GEOLOCATION,
                                 CONTENT_SETTING_ASK);
#if defined(ENABLE_NOTIFICATIONS)
  TestGrantAndRevoke_TestContent(content::PermissionType::NOTIFICATIONS,
                                 CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                                 CONTENT_SETTING_ASK);
#endif
  TestGrantAndRevoke_TestContent(content::PermissionType::MIDI_SYSEX,
                                 CONTENT_SETTINGS_TYPE_MIDI_SYSEX,
                                 CONTENT_SETTING_ASK);
  TestGrantAndRevoke_TestContent(content::PermissionType::PUSH_MESSAGING,
                                 CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                                 CONTENT_SETTING_ASK);
}
#endif

// Tests the global kill switch by enabling/disabling the Field Trials.
TEST_F(PermissionContextBaseTests, TestGlobalKillSwitch) {
  TestGlobalPermissionsKillSwitch(content::PermissionType::GEOLOCATION,
                                  CONTENT_SETTINGS_TYPE_GEOLOCATION);
  TestGlobalPermissionsKillSwitch(content::PermissionType::NOTIFICATIONS,
                                  CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
  TestGlobalPermissionsKillSwitch(content::PermissionType::MIDI_SYSEX,
                                  CONTENT_SETTINGS_TYPE_MIDI_SYSEX);
  TestGlobalPermissionsKillSwitch(content::PermissionType::PUSH_MESSAGING,
                                  CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
  TestGlobalPermissionsKillSwitch(content::PermissionType::DURABLE_STORAGE,
                                  CONTENT_SETTINGS_TYPE_DURABLE_STORAGE);
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
  TestGlobalPermissionsKillSwitch(
      content::PermissionType::PROTECTED_MEDIA_IDENTIFIER,
      CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER);
#endif
  TestGlobalPermissionsKillSwitch(content::PermissionType::AUDIO_CAPTURE,
                                  CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC);
  TestGlobalPermissionsKillSwitch(content::PermissionType::VIDEO_CAPTURE,
                                  CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA);
}

TEST_F(PermissionContextBaseTests, TestParallelRequestsAllowed) {
  TestParallelRequests(CONTENT_SETTING_ALLOW);
}

TEST_F(PermissionContextBaseTests, TestParallelRequestsBlocked) {
  TestParallelRequests(CONTENT_SETTING_BLOCK);
}

TEST_F(PermissionContextBaseTests, TestParallelRequestsDismissed) {
  TestParallelRequests(CONTENT_SETTING_ASK);
}
