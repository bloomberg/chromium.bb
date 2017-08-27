// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/geolocation/geolocation_permission_context.h"
#include "chrome/browser/media/midi_permission_context.h"
#include "chrome/browser/notifications/notification_permission_context.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "third_party/WebKit/public/platform/WebFeaturePolicyFeature.h"
#include "url/gurl.h"
#include "url/origin.h"

// Integration tests for querying permissions that have a feature policy set.
// These tests are not meant to cover every edge case as the FeaturePolicy class
// itself is tested thoroughly in feature_policy_unittest.cc and in
// render_frame_host_feature_policy_unittest.cc. Instead they are meant to
// ensure that integration with content::PermissionContextBase works correctly.
class PermissionContextBaseFeaturePolicyTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    feature_list_.InitAndEnableFeature(
        features::kUseFeaturePolicyForPermissions);
  }

 protected:
  static constexpr const char* kOrigin1 = "https://google.com";
  static constexpr const char* kOrigin2 = "https://maps.google.com";

  content::RenderFrameHost* GetMainRFH(const char* origin) {
    content::RenderFrameHost* result = web_contents()->GetMainFrame();
    content::RenderFrameHostTester::For(result)
        ->InitializeRenderFrameIfNeeded();
    SimulateNavigation(&result, GURL(origin));
    return result;
  }

  content::RenderFrameHost* AddChildRFH(content::RenderFrameHost* parent,
                                        const char* origin) {
    content::RenderFrameHost* result =
        content::RenderFrameHostTester::For(parent)->AppendChild("");
    content::RenderFrameHostTester::For(result)
        ->InitializeRenderFrameIfNeeded();
    SimulateNavigation(&result, GURL(origin));
    return result;
  }

  // The header policy should only be set once on page load, so we refresh the
  // page to simulate that.
  void RefreshPageAndSetHeaderPolicy(content::RenderFrameHost** rfh,
                                     blink::WebFeaturePolicyFeature feature,
                                     const std::vector<std::string>& origins) {
    content::RenderFrameHost* current = *rfh;
    SimulateNavigation(&current, current->GetLastCommittedURL());
    std::vector<url::Origin> parsed_origins;
    for (const std::string& origin : origins)
      parsed_origins.push_back(url::Origin(GURL(origin)));
    content::RenderFrameHostTester::For(current)->SimulateFeaturePolicyHeader(
        feature, parsed_origins);
    *rfh = current;
  }

  ContentSetting GetPermissionForFrame(PermissionContextBase* pcb,
                                       content::RenderFrameHost* rfh) {
    return pcb
        ->GetPermissionStatus(
            rfh, rfh->GetLastCommittedURL(),
            web_contents()->GetMainFrame()->GetLastCommittedURL())
        .content_setting;
  }

 private:
  void SimulateNavigation(content::RenderFrameHost** rfh, const GURL& url) {
    auto navigation_simulator =
        content::NavigationSimulator::CreateRendererInitiated(url, *rfh);
    navigation_simulator->Commit();
    *rfh = navigation_simulator->GetFinalRenderFrameHost();
  }

  base::test::ScopedFeatureList feature_list_;
};

// Feature policy should be ignored when the kUseFeaturePolicyForPermissions
// feature is disabled.
TEST_F(PermissionContextBaseFeaturePolicyTest, FeatureDisabled) {
  // Disable the feature.
  base::test::ScopedFeatureList feature_list;
  feature_list.Init();

  content::RenderFrameHost* parent = GetMainRFH(kOrigin1);

  RefreshPageAndSetHeaderPolicy(&parent,
                                blink::WebFeaturePolicyFeature::kMidiFeature,
                                std::vector<std::string>());
  MidiPermissionContext midi(profile());
  EXPECT_EQ(CONTENT_SETTING_ALLOW, GetPermissionForFrame(&midi, parent));

  RefreshPageAndSetHeaderPolicy(&parent,
                                blink::WebFeaturePolicyFeature::kGeolocation,
                                std::vector<std::string>());
  GeolocationPermissionContext geolocation(profile());
  EXPECT_EQ(CONTENT_SETTING_ASK, GetPermissionForFrame(&geolocation, parent));
}

TEST_F(PermissionContextBaseFeaturePolicyTest, DefaultPolicy) {
  content::RenderFrameHost* parent = GetMainRFH(kOrigin1);
  content::RenderFrameHost* child = AddChildRFH(parent, kOrigin2);

  // Midi is allowed by default in the top level frame but not in subframes.
  MidiPermissionContext midi(profile());
  EXPECT_EQ(CONTENT_SETTING_ALLOW, GetPermissionForFrame(&midi, parent));
  EXPECT_EQ(CONTENT_SETTING_BLOCK, GetPermissionForFrame(&midi, child));

  // Geolocation is ask by default in top level frames but not in subframes.
  GeolocationPermissionContext geolocation(profile());
  EXPECT_EQ(CONTENT_SETTING_ASK, GetPermissionForFrame(&geolocation, parent));
  EXPECT_EQ(CONTENT_SETTING_BLOCK, GetPermissionForFrame(&geolocation, child));

  // Notifications is ask by default in top level frames but not in subframes.
  NotificationPermissionContext notifications(
      profile(), CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
  EXPECT_EQ(CONTENT_SETTING_ASK, GetPermissionForFrame(&notifications, parent));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetPermissionForFrame(&notifications, child));
}

TEST_F(PermissionContextBaseFeaturePolicyTest, DisabledTopLevelFrame) {
  content::RenderFrameHost* parent = GetMainRFH(kOrigin1);

  // Disable midi in the top level frame.
  RefreshPageAndSetHeaderPolicy(&parent,
                                blink::WebFeaturePolicyFeature::kMidiFeature,
                                std::vector<std::string>());
  content::RenderFrameHost* child = AddChildRFH(parent, kOrigin2);
  MidiPermissionContext midi(profile());
  EXPECT_EQ(CONTENT_SETTING_BLOCK, GetPermissionForFrame(&midi, parent));
  EXPECT_EQ(CONTENT_SETTING_BLOCK, GetPermissionForFrame(&midi, child));

  // Disable geolocation in the top level frame.
  RefreshPageAndSetHeaderPolicy(&parent,
                                blink::WebFeaturePolicyFeature::kGeolocation,
                                std::vector<std::string>());
  child = AddChildRFH(parent, kOrigin2);
  GeolocationPermissionContext geolocation(profile());
  EXPECT_EQ(CONTENT_SETTING_BLOCK, GetPermissionForFrame(&geolocation, parent));
  EXPECT_EQ(CONTENT_SETTING_BLOCK, GetPermissionForFrame(&geolocation, child));
}

TEST_F(PermissionContextBaseFeaturePolicyTest, EnabledForChildFrame) {
  content::RenderFrameHost* parent = GetMainRFH(kOrigin1);

  // Enable midi for the child frame.
  RefreshPageAndSetHeaderPolicy(&parent,
                                blink::WebFeaturePolicyFeature::kMidiFeature,
                                {kOrigin1, kOrigin2});
  content::RenderFrameHost* child = AddChildRFH(parent, kOrigin2);
  MidiPermissionContext midi(profile());
  EXPECT_EQ(CONTENT_SETTING_ALLOW, GetPermissionForFrame(&midi, parent));
  EXPECT_EQ(CONTENT_SETTING_ALLOW, GetPermissionForFrame(&midi, child));

  // Enable geolocation for the child frame.
  RefreshPageAndSetHeaderPolicy(&parent,
                                blink::WebFeaturePolicyFeature::kGeolocation,
                                {kOrigin1, kOrigin2});
  child = AddChildRFH(parent, kOrigin2);
  GeolocationPermissionContext geolocation(profile());
  EXPECT_EQ(CONTENT_SETTING_ASK, GetPermissionForFrame(&geolocation, parent));
  EXPECT_EQ(CONTENT_SETTING_ASK, GetPermissionForFrame(&geolocation, child));
}
