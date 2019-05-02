// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background_sync/periodic_background_sync_permission_context.h"

#include <string>

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class PeriodicBackgroundSyncPermissionContextTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  PeriodicBackgroundSyncPermissionContextTest() = default;
  ~PeriodicBackgroundSyncPermissionContextTest() override = default;

  ContentSetting GetPermissionStatus(
      const GURL& url,
      PeriodicBackgroundSyncPermissionContext* permission_context,
      bool with_frame) {
    content::RenderFrameHost* render_frame_host = nullptr;

    if (with_frame) {
      content::WebContentsTester::For(web_contents())->NavigateAndCommit(url);
      render_frame_host = web_contents()->GetMainFrame();
    }

    auto permission_result = permission_context->GetPermissionStatus(
        render_frame_host, /* requesting_origin= */ url,
        /* embedding_origin= */ url);
    return permission_result.content_setting;
  }

  void SetPeriodicSyncContentSetting(const GURL& url, ContentSetting setting) {
    auto* host_content_settings_map =
        HostContentSettingsMapFactory::GetForProfile(profile());
    ASSERT_TRUE(host_content_settings_map);
    host_content_settings_map->SetContentSettingDefaultScope(
        /* primary_url= */ url, /* secondary_url= */ url,
        CONTENT_SETTINGS_TYPE_PERIODIC_BACKGROUND_SYNC,
        /* resource_identifier= */ std::string(), setting);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PeriodicBackgroundSyncPermissionContextTest);
};

TEST_F(PeriodicBackgroundSyncPermissionContextTest, DenyWhenFeatureDisabled) {
  PeriodicBackgroundSyncPermissionContext permission_context(profile());

  GURL url("https://example.com");
  EXPECT_EQ(GetPermissionStatus(url, &permission_context,
                                /* with_frame= */ true),
            CONTENT_SETTING_BLOCK);
}

TEST_F(PeriodicBackgroundSyncPermissionContextTest, DenyForInsecureOrigin) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kPeriodicBackgroundSync);

  PeriodicBackgroundSyncPermissionContext permission_context(profile());

  GURL url("http://example.com");
  SetPeriodicSyncContentSetting(url, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(GetPermissionStatus(url, &permission_context,
                                /* with_frame= */ false),
            CONTENT_SETTING_BLOCK);
}

TEST_F(PeriodicBackgroundSyncPermissionContextTest, AllowWithFrame) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kPeriodicBackgroundSync);

  GURL url("https://example.com");
  SetPeriodicSyncContentSetting(url, CONTENT_SETTING_ALLOW);

  PeriodicBackgroundSyncPermissionContext permission_context(profile());

  EXPECT_EQ(GetPermissionStatus(url, &permission_context,
                                /* with_frame= */ true),
            CONTENT_SETTING_ALLOW);
}

TEST_F(PeriodicBackgroundSyncPermissionContextTest, AllowWithoutFrame) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kPeriodicBackgroundSync);

  GURL url("https://example.com");
  SetPeriodicSyncContentSetting(url, CONTENT_SETTING_ALLOW);

  PeriodicBackgroundSyncPermissionContext permission_context(profile());

  EXPECT_EQ(GetPermissionStatus(url, &permission_context,
                                /* with_frame= */ false),
            CONTENT_SETTING_ALLOW);
}

}  // namespace
