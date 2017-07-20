// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_scene_manager.h"

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/elements/ui_element_debug_id.h"
#include "chrome/browser/vr/test/animation_utils.h"
#include "chrome/browser/vr/test/mock_browser_interface.h"
#include "chrome/browser/vr/test/ui_scene_manager_test.h"
#include "chrome/browser/vr/ui_scene.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using cc::TargetProperty::BOUNDS;
using cc::TargetProperty::TRANSFORM;

namespace vr {

namespace {
std::set<UiElementDebugId> kElementsVisibleInBrowsing = {
    kContentQuad, kBackplane, kCeiling, kFloor, kUrlBar};
std::set<UiElementDebugId> kElementsVisibleWithExitPrompt = {
    kExitPrompt, kExitPromptBackplane, kCeiling, kFloor};
}  // namespace

TEST_F(UiSceneManagerTest, ExitPresentAndFullscreenOnAppButtonClick) {
  MakeManager(kNotInCct, kInWebVr);

  // Clicking app button should trigger to exit presentation.
  EXPECT_CALL(*browser_, ExitPresent()).Times(1);
  // And also trigger exit fullscreen.
  EXPECT_CALL(*browser_, ExitFullscreen()).Times(1);
  manager_->OnAppButtonClicked();
}

TEST_F(UiSceneManagerTest, WebVrWarningsShowWhenInitiallyInWebVr) {
  MakeManager(kNotInCct, kInWebVr);

  EXPECT_TRUE(IsVisible(kWebVrPermanentHttpSecurityWarning));
  EXPECT_TRUE(IsVisible(kWebVrTransientHttpSecurityWarning));

  manager_->SetWebVrSecureOrigin(true);
  EXPECT_FALSE(IsVisible(kWebVrPermanentHttpSecurityWarning));
  EXPECT_FALSE(IsVisible(kWebVrTransientHttpSecurityWarning));

  manager_->SetWebVrSecureOrigin(false);
  EXPECT_TRUE(IsVisible(kWebVrPermanentHttpSecurityWarning));
  EXPECT_TRUE(IsVisible(kWebVrTransientHttpSecurityWarning));

  manager_->SetWebVrMode(false, false);
  EXPECT_FALSE(IsVisible(kWebVrPermanentHttpSecurityWarning));
  EXPECT_FALSE(IsVisible(kWebVrTransientHttpSecurityWarning));
}

TEST_F(UiSceneManagerTest, WebVrWarningsDoNotShowWhenInitiallyOutsideWebVr) {
  MakeManager(kNotInCct, kNotInWebVr);

  EXPECT_FALSE(IsVisible(kWebVrPermanentHttpSecurityWarning));
  EXPECT_FALSE(IsVisible(kWebVrTransientHttpSecurityWarning));

  manager_->SetWebVrMode(true, false);
  EXPECT_TRUE(IsVisible(kWebVrPermanentHttpSecurityWarning));
  EXPECT_TRUE(IsVisible(kWebVrTransientHttpSecurityWarning));
}

TEST_F(UiSceneManagerTest, WebVrTransientWarningTimesOut) {
  base::ScopedMockTimeMessageLoopTaskRunner task_runner_;

  MakeManager(kNotInCct, kInWebVr);
  EXPECT_TRUE(IsVisible(kWebVrTransientHttpSecurityWarning));

  // Note: Fast-forwarding appears broken in conjunction with restarting timers.
  // In this test, we can fast-forward by the appropriate time interval, but in
  // other cases (until the bug is addressed), we could work around the problem
  // by using FastForwardUntilNoTasksRemain() instead.
  // See http://crbug.com/736558.
  task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(31));
  EXPECT_FALSE(IsVisible(kWebVrTransientHttpSecurityWarning));
}

TEST_F(UiSceneManagerTest, ToastStateTransitions) {
  // Tests toast not showing when directly entering VR though WebVR
  // presentation.
  MakeManager(kNotInCct, kInWebVr);
  EXPECT_FALSE(IsVisible(kExclusiveScreenToast));

  MakeManager(kNotInCct, kNotInWebVr);
  EXPECT_FALSE(IsVisible(kExclusiveScreenToast));

  manager_->SetFullscreen(true);
  EXPECT_TRUE(IsVisible(kExclusiveScreenToast));

  manager_->SetWebVrMode(true, true);
  EXPECT_TRUE(IsVisible(kExclusiveScreenToast));

  manager_->SetWebVrMode(false, false);
  EXPECT_FALSE(IsVisible(kExclusiveScreenToast));

  manager_->SetFullscreen(false);
  EXPECT_FALSE(IsVisible(kExclusiveScreenToast));

  manager_->SetWebVrMode(true, false);
  EXPECT_FALSE(IsVisible(kExclusiveScreenToast));

  manager_->SetWebVrMode(false, true);
  EXPECT_FALSE(IsVisible(kExclusiveScreenToast));
}

TEST_F(UiSceneManagerTest, ToastTransience) {
  base::ScopedMockTimeMessageLoopTaskRunner task_runner_;

  MakeManager(kNotInCct, kNotInWebVr);
  EXPECT_FALSE(IsVisible(kExclusiveScreenToast));

  manager_->SetFullscreen(true);
  EXPECT_TRUE(IsVisible(kExclusiveScreenToast));
  task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(IsVisible(kExclusiveScreenToast));

  manager_->SetWebVrMode(true, true);
  EXPECT_TRUE(IsVisible(kExclusiveScreenToast));
  task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(IsVisible(kExclusiveScreenToast));

  manager_->SetWebVrMode(false, false);
  EXPECT_FALSE(IsVisible(kExclusiveScreenToast));
}

TEST_F(UiSceneManagerTest, CloseButtonVisibleInCctFullscreen) {
  // Button should be visible in cct.
  MakeManager(kInCct, kNotInWebVr);
  EXPECT_TRUE(IsVisible(kCloseButton));

  // Button should not be visible when not in cct or fullscreen.
  MakeManager(kNotInCct, kNotInWebVr);
  EXPECT_FALSE(IsVisible(kCloseButton));

  // Button should be visible in fullscreen and hidden when leaving fullscreen.
  manager_->SetFullscreen(true);
  EXPECT_TRUE(IsVisible(kCloseButton));
  manager_->SetFullscreen(false);
  EXPECT_FALSE(IsVisible(kCloseButton));

  // Button should not be visible when in WebVR.
  MakeManager(kInCct, kInWebVr);
  EXPECT_FALSE(IsVisible(kCloseButton));
  manager_->SetWebVrMode(false, false);
  EXPECT_TRUE(IsVisible(kCloseButton));

  // Button should be visible in Cct across transistions in fullscreen.
  MakeManager(kInCct, kNotInWebVr);
  EXPECT_TRUE(IsVisible(kCloseButton));
  manager_->SetFullscreen(true);
  EXPECT_TRUE(IsVisible(kCloseButton));
  manager_->SetFullscreen(false);
  EXPECT_TRUE(IsVisible(kCloseButton));
}

TEST_F(UiSceneManagerTest, UiUpdatesForIncognito) {
  MakeManager(kNotInCct, kNotInWebVr);

  // Hold onto the background color to make sure it changes.
  SkColor initial_background = scene_->GetWorldBackgroundColor();
  manager_->SetFullscreen(true);

  {
    SCOPED_TRACE("Entered Fullsceen");
    // Make sure background has changed for fullscreen.
    EXPECT_NE(initial_background, scene_->GetWorldBackgroundColor());
  }

  SkColor fullscreen_background = scene_->GetWorldBackgroundColor();

  manager_->SetIncognito(true);

  {
    SCOPED_TRACE("Entered Incognito");
    // Make sure background has changed for incognito.
    EXPECT_NE(fullscreen_background, scene_->GetWorldBackgroundColor());
    EXPECT_NE(initial_background, scene_->GetWorldBackgroundColor());
  }

  SkColor incognito_background = scene_->GetWorldBackgroundColor();

  manager_->SetIncognito(false);

  {
    SCOPED_TRACE("Exited Incognito");
    EXPECT_EQ(fullscreen_background, scene_->GetWorldBackgroundColor());
  }

  manager_->SetFullscreen(false);

  {
    SCOPED_TRACE("Exited Fullsceen");
    EXPECT_EQ(initial_background, scene_->GetWorldBackgroundColor());
  }

  manager_->SetIncognito(true);

  {
    SCOPED_TRACE("Entered Incognito");
    EXPECT_EQ(incognito_background, scene_->GetWorldBackgroundColor());
  }

  manager_->SetIncognito(false);

  {
    SCOPED_TRACE("Exited Incognito");
    EXPECT_EQ(initial_background, scene_->GetWorldBackgroundColor());
  }
}

TEST_F(UiSceneManagerTest, WebVrAutopresentedInsecureOrigin) {
  base::ScopedMockTimeMessageLoopTaskRunner task_runner_;

  MakeAutoPresentedManager();
  manager_->SetWebVrSecureOrigin(false);
  manager_->SetWebVrMode(true, false);
  // Initially, the security warnings should not be visible since the first
  // WebVR frame is not received.
  VerifyElementsVisible("Initial",
                        std::set<UiElementDebugId>{kSplashScreenIcon});
  manager_->OnWebVrFrameAvailable();
  VerifyElementsVisible("Autopresented", std::set<UiElementDebugId>{
                                             kWebVrPermanentHttpSecurityWarning,
                                             kWebVrTransientHttpSecurityWarning,
                                             kTransientUrlBar});

  // Make sure the transient elements go away.
  task_runner_->FastForwardUntilNoTasksRemain();
  VerifyElementsVisible("End state", std::set<UiElementDebugId>{
                                         kWebVrPermanentHttpSecurityWarning});
}

TEST_F(UiSceneManagerTest, WebVrAutopresented) {
  base::ScopedMockTimeMessageLoopTaskRunner task_runner_;

  MakeAutoPresentedManager();
  manager_->SetWebVrSecureOrigin(true);

  // Initially, we should only show the splash screen.
  VerifyElementsVisible("Initial",
                        std::set<UiElementDebugId>{kSplashScreenIcon});

  // Enter WebVR with autopresentation.
  manager_->SetWebVrMode(true, false);
  manager_->OnWebVrFrameAvailable();
  VerifyElementsVisible("Autopresented",
                        std::set<UiElementDebugId>{kTransientUrlBar});

  // Make sure the transient URL bar times out.
  task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(IsVisible(kTransientUrlBar));
}

TEST_F(UiSceneManagerTest, UiUpdatesForFullscreenChanges) {
  std::set<UiElementDebugId> visible_in_fullscreen = {
      kContentQuad, kCloseButton, kBackplane,
      kCeiling,     kFloor,       kExclusiveScreenToast,
  };

  MakeManager(kNotInCct, kNotInWebVr);

  // Hold onto the background color to make sure it changes.
  SkColor initial_background = scene_->GetWorldBackgroundColor();
  VerifyElementsVisible("Initial", kElementsVisibleInBrowsing);
  UiElement* content_quad = scene_->GetUiElementByDebugId(kContentQuad);
  gfx::SizeF initial_content_size = content_quad->size();
  gfx::Transform initial_position =
      content_quad->transform_operations().Apply();

  // In fullscreen mode, content elements should be visible, control elements
  // should be hidden.
  manager_->SetFullscreen(true);
  VerifyElementsVisible("In fullscreen", visible_in_fullscreen);
  // Make sure background has changed for fullscreen.
  EXPECT_NE(initial_background, scene_->GetWorldBackgroundColor());
  // Should have started transition.
  EXPECT_TRUE(IsAnimating(content_quad, {TRANSFORM, BOUNDS}));
  // Finish the transition.
  AnimateBy(MsToDelta(1000));
  EXPECT_FALSE(IsAnimating(content_quad, {TRANSFORM, BOUNDS}));
  EXPECT_NE(initial_content_size, content_quad->size());
  EXPECT_NE(initial_position, content_quad->transform_operations().Apply());

  // Everything should return to original state after leaving fullscreen.
  manager_->SetFullscreen(false);
  VerifyElementsVisible("Restore initial", kElementsVisibleInBrowsing);
  EXPECT_EQ(initial_background, scene_->GetWorldBackgroundColor());
  // Should have started transition.
  EXPECT_TRUE(IsAnimating(content_quad, {TRANSFORM, BOUNDS}));
  // Finish the transition.
  AnimateBy(MsToDelta(1000));
  EXPECT_FALSE(IsAnimating(content_quad, {TRANSFORM, BOUNDS}));
  EXPECT_EQ(initial_content_size, content_quad->size());
  EXPECT_EQ(initial_position, content_quad->transform_operations().Apply());
}

TEST_F(UiSceneManagerTest, UiUpdatesExitPrompt) {
  MakeManager(kNotInCct, kNotInWebVr);

  manager_->SetWebVrSecureOrigin(true);

  // Initial state.
  VerifyElementsVisible("Initial", kElementsVisibleInBrowsing);

  // Exit prompt visible state.
  manager_->OnSecurityIconClickedForTesting();
  VerifyElementsVisible("Prompt visible", kElementsVisibleWithExitPrompt);

  // Back to initial state.
  manager_->OnExitPromptPrimaryButtonClickedForTesting();
  VerifyElementsVisible("Restore initial", kElementsVisibleInBrowsing);
}

TEST_F(UiSceneManagerTest, BackplaneClickClosesExitPrompt) {
  MakeManager(kNotInCct, kNotInWebVr);

  manager_->SetWebVrSecureOrigin(true);

  // Initial state.
  VerifyElementsVisible("Initial", kElementsVisibleInBrowsing);

  // Exit prompt visible state.
  manager_->OnSecurityIconClickedForTesting();
  VerifyElementsVisible("Prompt visble", kElementsVisibleWithExitPrompt);

  // Back to initial state.
  scene_->GetUiElementByDebugId(kExitPromptBackplane)
      ->OnButtonUp(gfx::PointF());
  VerifyElementsVisible("Restore initial", kElementsVisibleInBrowsing);
}

TEST_F(UiSceneManagerTest, UiUpdatesForWebVR) {
  MakeManager(kNotInCct, kInWebVr);

  manager_->SetWebVrSecureOrigin(true);
  manager_->SetAudioCapturingIndicator(true);
  manager_->SetVideoCapturingIndicator(true);
  manager_->SetScreenCapturingIndicator(true);
  manager_->SetLocationAccessIndicator(true);
  manager_->SetBluetoothConnectedIndicator(true);

  // All elements should be hidden.
  VerifyElementsVisible("Elements hidden", std::set<UiElementDebugId>{});
}

TEST_F(UiSceneManagerTest, UiUpdateTransitionToWebVR) {
  MakeManager(kNotInCct, kNotInWebVr);
  manager_->SetAudioCapturingIndicator(true);
  manager_->SetVideoCapturingIndicator(true);
  manager_->SetScreenCapturingIndicator(true);
  manager_->SetLocationAccessIndicator(true);
  manager_->SetBluetoothConnectedIndicator(true);

  // Transition to WebVR mode
  manager_->SetWebVrMode(true, false);
  manager_->SetWebVrSecureOrigin(true);

  // All elements should be hidden.
  VerifyElementsVisible("Elements hidden", std::set<UiElementDebugId>{});
}

TEST_F(UiSceneManagerTest, CaptureIndicatorsVisibility) {
  const std::set<UiElementDebugId> indicators = {
      kAudioCaptureIndicator,       kVideoCaptureIndicator,
      kScreenCaptureIndicator,      kLocationAccessIndicator,
      kBluetoothConnectedIndicator,
  };

  MakeManager(kNotInCct, kNotInWebVr);
  EXPECT_TRUE(VerifyVisibility(indicators, false));

  manager_->SetAudioCapturingIndicator(true);
  manager_->SetVideoCapturingIndicator(true);
  manager_->SetScreenCapturingIndicator(true);
  manager_->SetLocationAccessIndicator(true);
  manager_->SetBluetoothConnectedIndicator(true);
  EXPECT_TRUE(VerifyVisibility(indicators, true));

  // Go into non-browser modes and make sure all indicators are hidden.
  manager_->SetWebVrMode(true, false);
  EXPECT_TRUE(VerifyVisibility(indicators, false));
  manager_->SetWebVrMode(false, false);
  manager_->SetFullscreen(true);
  EXPECT_TRUE(VerifyVisibility(indicators, false));
  manager_->SetFullscreen(false);

  // Back to browser, make sure the indicators reappear.
  EXPECT_TRUE(VerifyVisibility(indicators, true));

  // Ensure they can be turned off.
  manager_->SetAudioCapturingIndicator(false);
  manager_->SetVideoCapturingIndicator(false);
  manager_->SetScreenCapturingIndicator(false);
  manager_->SetLocationAccessIndicator(false);
  manager_->SetBluetoothConnectedIndicator(false);
  EXPECT_TRUE(VerifyVisibility(indicators, false));
}

}  // namespace vr
