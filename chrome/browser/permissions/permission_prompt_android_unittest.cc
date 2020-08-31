// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/infobars/mock_infobar_service.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/mock_permission_request.h"
#include "components/prefs/pref_service.h"

class PermissionPromptAndroidTest : public ChromeRenderViewHostTestHarness {
 public:
  permissions::PermissionRequestManager* permission_request_manager() {
    return permission_request_manager_;
  }

 private:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    // Ensure that the test uses the mini-infobar variant.
    scoped_feature_list_.InitAndEnableFeature(
        features::kQuietNotificationPrompts);
    profile()->GetPrefs()->SetBoolean(
        prefs::kEnableQuietNotificationPermissionUi, true);

    NavigateAndCommit(GURL("http://example.com"));

    MockInfoBarService::CreateForWebContents(web_contents());

    permissions::PermissionRequestManager::CreateForWebContents(web_contents());
    permission_request_manager_ =
        permissions::PermissionRequestManager::FromWebContents(web_contents());
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  permissions::PermissionRequestManager* permission_request_manager_;
};

// Tests the situation in crbug.com/1016233
TEST_F(PermissionPromptAndroidTest, TabCloseMiniInfoBarClosesCleanly) {
  // Create a notification request. This causes an infobar to appear.
  permissions::MockPermissionRequest request(
      "test", ContentSettingsType::NOTIFICATIONS);
  permission_request_manager()->AddRequest(&request);

  base::RunLoop().RunUntilIdle();

  // Now remove the infobar from the infobar service.
  InfoBarService::FromWebContents(web_contents())->RemoveAllInfoBars(false);

  // At this point close the permission prompt (after the infobar has been
  // removed already).
  permission_request_manager()->Deny();

  // If no DCHECK has been hit, and the infobar has been closed, the test
  // passes.
  EXPECT_TRUE(request.finished());
}

// Tests the situation in crbug.com/1016233
TEST_F(PermissionPromptAndroidTest, RemoveAllInfoBarsWithOtherObservers) {
  // Create a notification request. This causes an infobar to appear.
  permissions::MockPermissionRequest request(
      "test", ContentSettingsType::NOTIFICATIONS);
  permission_request_manager()->AddRequest(&request);

  base::RunLoop().RunUntilIdle();

  // Destroy web contents. This triggered the situation in crbug.com/1016233, as
  // it causes the destruction of the permission prompt after the destruction of
  // the infobar manager.
  DeleteContents();

  // Wait for all the WebContentsObserver's to handle the fact that the
  // WebContents has been destroyed.
  base::RunLoop().RunUntilIdle();

  // If no crash occurs, the test passes.
}
