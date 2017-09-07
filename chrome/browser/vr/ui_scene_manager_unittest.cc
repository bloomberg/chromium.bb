// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_scene_manager.h"

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "cc/base/math_util.h"
#include "chrome/browser/vr/color_scheme.h"
#include "chrome/browser/vr/elements/content_element.h"
#include "chrome/browser/vr/elements/grid.h"
#include "chrome/browser/vr/elements/rect.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/elements/ui_element_name.h"
#include "chrome/browser/vr/target_property.h"
#include "chrome/browser/vr/test/animation_utils.h"
#include "chrome/browser/vr/test/constants.h"
#include "chrome/browser/vr/test/fake_ui_element_renderer.h"
#include "chrome/browser/vr/test/mock_browser_interface.h"
#include "chrome/browser/vr/test/ui_scene_manager_test.h"
#include "chrome/browser/vr/ui_scene.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace vr {

using TargetProperty::BOUNDS;
using TargetProperty::TRANSFORM;
using TargetProperty::OPACITY;

namespace {
std::set<UiElementName> kBackgroundElements = {
    kBackgroundFront, kBackgroundLeft, kBackgroundBack,
    kBackgroundRight, kBackgroundTop,  kBackgroundBottom};
std::set<UiElementName> kFloorCeilingBackgroundElements = {
    kBackgroundFront, kBackgroundLeft,   kBackgroundBack, kBackgroundRight,
    kBackgroundTop,   kBackgroundBottom, kCeiling,        kFloor};
std::set<UiElementName> kElementsVisibleInBrowsing = {
    kBackgroundFront, kBackgroundLeft, kBackgroundBack,
    kBackgroundRight, kBackgroundTop,  kBackgroundBottom,
    kCeiling,         kFloor,          kContentQuad,
    kBackplane,       kUrlBar,         kUnderDevelopmentNotice};
std::set<UiElementName> kElementsVisibleWithExitPrompt = {
    kBackgroundFront, kBackgroundLeft,     kBackgroundBack, kBackgroundRight,
    kBackgroundTop,   kBackgroundBottom,   kCeiling,        kFloor,
    kExitPrompt,      kExitPromptBackplane};
std::set<UiElementName> kHitTestableElements = {
    kFloor,
    kCeiling,
    kBackplane,
    kContentQuad,
    kAudioCaptureIndicator,
    kVideoCaptureIndicator,
    kScreenCaptureIndicator,
    kBluetoothConnectedIndicator,
    kLocationAccessIndicator,
    kExitPrompt,
    kExitPromptBackplane,
    kUrlBar,
    kLoadingIndicator,
    kCloseButton,
};

static constexpr float kTolerance = 1e-5;

MATCHER_P2(SizeFsAreApproximatelyEqual, other, tolerance, "") {
  return cc::MathUtil::ApproximatelyEqual(arg.width(), other.width(),
                                          tolerance) &&
         cc::MathUtil::ApproximatelyEqual(arg.height(), other.height(),
                                          tolerance);
}

void CheckHitTestableRecursive(UiElement* element) {
  const bool should_be_hit_testable =
      kHitTestableElements.find(element->name()) != kHitTestableElements.end();
  EXPECT_EQ(should_be_hit_testable, element->hit_testable())
      << "element name: " << element->name();
  for (const auto& child : element->children()) {
    CheckHitTestableRecursive(child.get());
  }
}

}  // namespace

TEST_F(UiSceneManagerTest, ExitPresentAndFullscreenOnAppButtonClick) {
  MakeManager(kNotInCct, kInWebVr);

  // Clicking app button should trigger to exit presentation.
  EXPECT_CALL(*browser_, ExitPresent());
  // And also trigger exit fullscreen.
  EXPECT_CALL(*browser_, ExitFullscreen());
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
  EXPECT_FALSE(IsVisible(kExclusiveScreenToastViewportAware));

  MakeManager(kNotInCct, kNotInWebVr);
  EXPECT_FALSE(IsVisible(kExclusiveScreenToast));
  EXPECT_FALSE(IsVisible(kExclusiveScreenToastViewportAware));

  manager_->SetFullscreen(true);
  EXPECT_TRUE(IsVisible(kExclusiveScreenToast));
  EXPECT_FALSE(IsVisible(kExclusiveScreenToastViewportAware));

  manager_->SetWebVrMode(true, true);
  EXPECT_FALSE(IsVisible(kExclusiveScreenToast));
  EXPECT_TRUE(IsVisible(kExclusiveScreenToastViewportAware));

  manager_->SetWebVrMode(false, false);
  EXPECT_FALSE(IsVisible(kExclusiveScreenToast));
  EXPECT_FALSE(IsVisible(kExclusiveScreenToastViewportAware));

  manager_->SetFullscreen(false);
  EXPECT_FALSE(IsVisible(kExclusiveScreenToast));
  EXPECT_FALSE(IsVisible(kExclusiveScreenToastViewportAware));

  manager_->SetWebVrMode(true, false);
  EXPECT_FALSE(IsVisible(kExclusiveScreenToast));
  EXPECT_FALSE(IsVisible(kExclusiveScreenToastViewportAware));

  manager_->SetWebVrMode(false, true);
  EXPECT_FALSE(IsVisible(kExclusiveScreenToast));
  EXPECT_FALSE(IsVisible(kExclusiveScreenToastViewportAware));
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
  EXPECT_TRUE(IsVisible(kExclusiveScreenToastViewportAware));
  task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(IsVisible(kExclusiveScreenToastViewportAware));

  manager_->SetWebVrMode(false, false);
  EXPECT_FALSE(IsVisible(kExclusiveScreenToastViewportAware));
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
  SkColor initial_background = GetBackgroundColor();
  manager_->SetFullscreen(true);

  // Make sure background has changed for fullscreen.
  EXPECT_NE(initial_background, GetBackgroundColor());

  SkColor fullscreen_background = GetBackgroundColor();

  manager_->SetIncognito(true);

  // Make sure background has changed for incognito.
  EXPECT_NE(fullscreen_background, GetBackgroundColor());
  EXPECT_NE(initial_background, GetBackgroundColor());

  SkColor incognito_background = GetBackgroundColor();

  manager_->SetIncognito(false);
  EXPECT_EQ(fullscreen_background, GetBackgroundColor());

  manager_->SetFullscreen(false);
  EXPECT_EQ(initial_background, GetBackgroundColor());

  manager_->SetIncognito(true);
  EXPECT_EQ(incognito_background, GetBackgroundColor());

  manager_->SetIncognito(false);
  EXPECT_EQ(initial_background, GetBackgroundColor());
}

TEST_F(UiSceneManagerTest, WebVrAutopresentedInsecureOrigin) {
  base::ScopedMockTimeMessageLoopTaskRunner task_runner_;

  MakeAutoPresentedManager();
  manager_->SetWebVrSecureOrigin(false);
  manager_->SetWebVrMode(true, false);
  // Initially, the security warnings should not be visible since the first
  // WebVR frame is not received.
  auto initial_elements = kBackgroundElements;
  initial_elements.insert(kSplashScreenText);
  VerifyElementsVisible("Initial", initial_elements);

  manager_->OnWebVrFrameAvailable();
  // Start the transition, but don't finish it.
  AnimateBy(MsToDelta(50));
  VerifyElementsVisible(
      "Autopresented", std::set<UiElementName>{
                           kWebVrPermanentHttpSecurityWarning,
                           kWebVrTransientHttpSecurityWarning, kWebVrUrlToast});

  // Make sure the transient elements go away.
  task_runner_->FastForwardUntilNoTasksRemain();
  UiElement* transient_url_bar = scene_->GetUiElementByName(kWebVrUrlToast);
  EXPECT_TRUE(IsAnimating(transient_url_bar, {OPACITY}));
  // Finish the transition.
  AnimateBy(MsToDelta(1000));
  EXPECT_FALSE(IsAnimating(transient_url_bar, {OPACITY}));
  VerifyElementsVisible(
      "End state", std::set<UiElementName>{kWebVrPermanentHttpSecurityWarning});
}

TEST_F(UiSceneManagerTest, WebVrAutopresented) {
  base::ScopedMockTimeMessageLoopTaskRunner task_runner_;

  MakeAutoPresentedManager();
  manager_->SetWebVrSecureOrigin(true);

  // Initially, we should only show the splash screen.
  auto initial_elements = kBackgroundElements;
  initial_elements.insert(kSplashScreenText);
  VerifyElementsVisible("Initial", initial_elements);
  EXPECT_EQ(ColorScheme::GetColorScheme(ColorScheme::kModeNormal)
                .splash_screen_background,
            GetBackgroundColor());

  // Enter WebVR with autopresentation.
  manager_->SetWebVrMode(true, false);
  manager_->OnWebVrFrameAvailable();
  // Start the transition, but don't finish it.
  AnimateBy(MsToDelta(50));
  VerifyElementsVisible("Autopresented",
                        std::set<UiElementName>{kWebVrUrlToast});

  // Make sure the transient URL bar times out.
  task_runner_->FastForwardUntilNoTasksRemain();
  UiElement* transient_url_bar = scene_->GetUiElementByName(kWebVrUrlToast);
  EXPECT_TRUE(
      IsAnimating(transient_url_bar, {OPACITY}));  // Finish the transition.
  AnimateBy(MsToDelta(1000));
  EXPECT_FALSE(IsAnimating(transient_url_bar, {OPACITY}));
  EXPECT_FALSE(IsVisible(kWebVrUrlToast));
}

TEST_F(UiSceneManagerTest, AppButtonClickForAutopresentation) {
  MakeAutoPresentedManager();

  // Clicking app button should be a no-op.
  EXPECT_CALL(*browser_, ExitPresent()).Times(0);
  EXPECT_CALL(*browser_, ExitFullscreen()).Times(0);
  manager_->OnAppButtonClicked();
}

TEST_F(UiSceneManagerTest, UiUpdatesForFullscreenChanges) {
  auto visible_in_fullscreen = kFloorCeilingBackgroundElements;
  visible_in_fullscreen.insert(kContentQuad);
  visible_in_fullscreen.insert(kBackplane);
  visible_in_fullscreen.insert(kCloseButton);
  visible_in_fullscreen.insert(kExclusiveScreenToast);

  MakeManager(kNotInCct, kNotInWebVr);

  // Hold onto the background color to make sure it changes.
  SkColor initial_background = GetBackgroundColor();
  VerifyElementsVisible("Initial", kElementsVisibleInBrowsing);
  UiElement* content_quad = scene_->GetUiElementByName(kContentQuad);
  UiElement* content_group =
      scene_->GetUiElementByName(k2dBrowsingContentGroup);
  gfx::SizeF initial_content_size = content_quad->size();
  gfx::Transform initial_position = content_group->LocalTransform();

  // In fullscreen mode, content elements should be visible, control elements
  // should be hidden.
  manager_->SetFullscreen(true);
  VerifyElementsVisible("In fullscreen", visible_in_fullscreen);
  // Make sure background has changed for fullscreen.
  EXPECT_NE(initial_background, GetBackgroundColor());
  // Should have started transition.
  EXPECT_TRUE(IsAnimating(content_quad, {BOUNDS}));
  EXPECT_TRUE(IsAnimating(content_group, {TRANSFORM}));
  // Finish the transition.
  AnimateBy(MsToDelta(1000));
  EXPECT_FALSE(IsAnimating(content_quad, {BOUNDS}));
  EXPECT_FALSE(IsAnimating(content_group, {TRANSFORM}));
  EXPECT_NE(initial_content_size, content_quad->size());
  EXPECT_NE(initial_position, content_group->LocalTransform());

  // Everything should return to original state after leaving fullscreen.
  manager_->SetFullscreen(false);
  VerifyElementsVisible("Restore initial", kElementsVisibleInBrowsing);
  EXPECT_EQ(initial_background, GetBackgroundColor());
  // Should have started transition.
  EXPECT_TRUE(IsAnimating(content_quad, {BOUNDS}));
  EXPECT_TRUE(IsAnimating(content_group, {TRANSFORM}));
  // Finish the transition.
  AnimateBy(MsToDelta(1000));
  EXPECT_FALSE(IsAnimating(content_quad, {BOUNDS}));
  EXPECT_FALSE(IsAnimating(content_group, {TRANSFORM}));
  EXPECT_EQ(initial_content_size, content_quad->size());
  EXPECT_EQ(initial_position, content_group->LocalTransform());
}

TEST_F(UiSceneManagerTest, SecurityIconClickTriggersUnsupportedMode) {
  MakeManager(kNotInCct, kNotInWebVr);

  manager_->SetWebVrSecureOrigin(true);

  // Initial state.
  VerifyElementsVisible("Initial", kElementsVisibleInBrowsing);

  // Clicking on security icon should trigger unsupported mode.
  EXPECT_CALL(*browser_,
              OnUnsupportedMode(UiUnsupportedMode::kUnhandledPageInfo));
  manager_->OnSecurityIconClickedForTesting();
  VerifyElementsVisible("Prompt invisible", kElementsVisibleInBrowsing);
}

TEST_F(UiSceneManagerTest, UiUpdatesForShowingExitPrompt) {
  MakeManager(kNotInCct, kNotInWebVr);

  manager_->SetWebVrSecureOrigin(true);

  // Initial state.
  VerifyElementsVisible("Initial", kElementsVisibleInBrowsing);

  // Showing exit VR prompt should make prompt visible.
  manager_->SetExitVrPromptEnabled(true, UiUnsupportedMode::kUnhandledPageInfo);
  VerifyElementsVisible("Prompt visible", kElementsVisibleWithExitPrompt);
}

TEST_F(UiSceneManagerTest, UiUpdatesForHidingExitPrompt) {
  MakeManager(kNotInCct, kNotInWebVr);

  manager_->SetWebVrSecureOrigin(true);

  // Initial state.
  manager_->SetExitVrPromptEnabled(true, UiUnsupportedMode::kUnhandledPageInfo);
  VerifyElementsVisible("Initial", kElementsVisibleWithExitPrompt);

  // Hiding exit VR prompt should make prompt invisible.
  manager_->SetExitVrPromptEnabled(false, UiUnsupportedMode::kCount);
  VerifyElementsVisible("Prompt invisible", kElementsVisibleInBrowsing);
}

TEST_F(UiSceneManagerTest, BackplaneClickTriggersOnExitPrompt) {
  MakeManager(kNotInCct, kNotInWebVr);

  manager_->SetWebVrSecureOrigin(true);

  // Initial state.
  VerifyElementsVisible("Initial", kElementsVisibleInBrowsing);
  manager_->SetExitVrPromptEnabled(true, UiUnsupportedMode::kUnhandledPageInfo);

  // Click on backplane should trigger UI browser interface but not close
  // prompt.
  EXPECT_CALL(*browser_,
              OnExitVrPromptResult(UiUnsupportedMode::kUnhandledPageInfo,
                                   ExitVrPromptChoice::CHOICE_NONE));
  scene_->GetUiElementByName(kExitPromptBackplane)->OnButtonUp(gfx::PointF());
  VerifyElementsVisible("Prompt still visible", kElementsVisibleWithExitPrompt);
}

TEST_F(UiSceneManagerTest, PrimaryButtonClickTriggersOnExitPrompt) {
  MakeManager(kNotInCct, kNotInWebVr);

  manager_->SetWebVrSecureOrigin(true);

  // Initial state.
  VerifyElementsVisible("Initial", kElementsVisibleInBrowsing);
  manager_->SetExitVrPromptEnabled(true, UiUnsupportedMode::kUnhandledPageInfo);

  // Click on 'OK' should trigger UI browser interface but not close prompt.
  EXPECT_CALL(*browser_,
              OnExitVrPromptResult(UiUnsupportedMode::kUnhandledPageInfo,
                                   ExitVrPromptChoice::CHOICE_STAY));
  manager_->OnExitPromptChoiceForTesting(false);
  VerifyElementsVisible("Prompt still visible", kElementsVisibleWithExitPrompt);
}

TEST_F(UiSceneManagerTest, SecondaryButtonClickTriggersOnExitPrompt) {
  MakeManager(kNotInCct, kNotInWebVr);

  manager_->SetWebVrSecureOrigin(true);

  // Initial state.
  VerifyElementsVisible("Initial", kElementsVisibleInBrowsing);
  manager_->SetExitVrPromptEnabled(true, UiUnsupportedMode::kUnhandledPageInfo);

  // Click on 'Exit VR' should trigger UI browser interface but not close
  // prompt.
  EXPECT_CALL(*browser_,
              OnExitVrPromptResult(UiUnsupportedMode::kUnhandledPageInfo,
                                   ExitVrPromptChoice::CHOICE_EXIT));
  manager_->OnExitPromptChoiceForTesting(true);
  VerifyElementsVisible("Prompt still visible", kElementsVisibleWithExitPrompt);
}

TEST_F(UiSceneManagerTest, SecondExitPromptTriggersOnExitPrompt) {
  MakeManager(kNotInCct, kNotInWebVr);

  manager_->SetWebVrSecureOrigin(true);

  // Initial state.
  VerifyElementsVisible("Initial", kElementsVisibleInBrowsing);
  manager_->SetExitVrPromptEnabled(true, UiUnsupportedMode::kUnhandledPageInfo);

  // Click on 'Exit VR' should trigger UI browser interface but not close
  // prompt.
  EXPECT_CALL(*browser_,
              OnExitVrPromptResult(UiUnsupportedMode::kUnhandledPageInfo,
                                   ExitVrPromptChoice::CHOICE_NONE));
  manager_->SetExitVrPromptEnabled(true,
                                   UiUnsupportedMode::kUnhandledCodePoint);
  VerifyElementsVisible("Prompt still visible", kElementsVisibleWithExitPrompt);
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
  VerifyElementsVisible("Elements hidden", std::set<UiElementName>{});
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
  VerifyElementsVisible("Elements hidden", std::set<UiElementName>{});
}

TEST_F(UiSceneManagerTest, CaptureIndicatorsVisibility) {
  const std::set<UiElementName> indicators = {
      kAudioCaptureIndicator,       kVideoCaptureIndicator,
      kScreenCaptureIndicator,      kLocationAccessIndicator,
      kBluetoothConnectedIndicator,
  };

  MakeManager(kNotInCct, kNotInWebVr);
  EXPECT_TRUE(VerifyVisibility(indicators, false));
  EXPECT_TRUE(VerifyRequiresLayout(indicators, false));

  manager_->SetAudioCapturingIndicator(true);
  manager_->SetVideoCapturingIndicator(true);
  manager_->SetScreenCapturingIndicator(true);
  manager_->SetLocationAccessIndicator(true);
  manager_->SetBluetoothConnectedIndicator(true);
  EXPECT_TRUE(VerifyVisibility(indicators, true));
  EXPECT_TRUE(VerifyRequiresLayout(indicators, true));

  // Go into non-browser modes and make sure all indicators are hidden.
  manager_->SetWebVrMode(true, false);
  EXPECT_TRUE(VerifyVisibility(indicators, false));
  EXPECT_TRUE(VerifyRequiresLayout(indicators, false));
  manager_->SetWebVrMode(false, false);
  manager_->SetFullscreen(true);
  EXPECT_TRUE(VerifyVisibility(indicators, false));
  EXPECT_TRUE(VerifyRequiresLayout(indicators, false));
  manager_->SetFullscreen(false);

  // Back to browser, make sure the indicators reappear.
  EXPECT_TRUE(VerifyVisibility(indicators, true));
  EXPECT_TRUE(VerifyRequiresLayout(indicators, true));

  // Ensure they can be turned off.
  manager_->SetAudioCapturingIndicator(false);
  manager_->SetVideoCapturingIndicator(false);
  manager_->SetScreenCapturingIndicator(false);
  manager_->SetLocationAccessIndicator(false);
  manager_->SetBluetoothConnectedIndicator(false);
  EXPECT_TRUE(VerifyVisibility(indicators, false));
  EXPECT_TRUE(VerifyRequiresLayout(indicators, false));
}

TEST_F(UiSceneManagerTest, PropagateContentBoundsOnStart) {
  MakeManager(kNotInCct, kNotInWebVr);

  // Calculate the inheritable transforms.
  AnimateBy(MsToDelta(0));

  gfx::SizeF expected_bounds(0.495922f, 0.330614f);
  EXPECT_CALL(*browser_,
              OnContentScreenBoundsChanged(
                  SizeFsAreApproximatelyEqual(expected_bounds, kTolerance)));

  manager_->OnProjMatrixChanged(kProjMatrix);
}

TEST_F(UiSceneManagerTest, PropagateContentBoundsOnFullscreen) {
  MakeManager(kNotInCct, kNotInWebVr);

  AnimateBy(MsToDelta(0));
  manager_->OnProjMatrixChanged(kProjMatrix);
  manager_->SetFullscreen(true);
  AnimateBy(MsToDelta(0));

  gfx::SizeF expected_bounds(0.705449f, 0.396737f);
  EXPECT_CALL(*browser_,
              OnContentScreenBoundsChanged(
                  SizeFsAreApproximatelyEqual(expected_bounds, kTolerance)));

  manager_->OnProjMatrixChanged(kProjMatrix);
}

TEST_F(UiSceneManagerTest, HitTestableElements) {
  MakeManager(kNotInCct, kNotInWebVr);
  AnimateBy(MsToDelta(0));
  CheckHitTestableRecursive(&scene_->root_element());
}

TEST_F(UiSceneManagerTest, DontPropagateContentBoundsOnNegligibleChange) {
  MakeManager(kNotInCct, kNotInWebVr);

  AnimateBy(MsToDelta(0));
  manager_->OnProjMatrixChanged(kProjMatrix);

  UiElement* content_quad = scene_->GetUiElementByName(kContentQuad);
  gfx::SizeF content_quad_size = content_quad->size();
  content_quad_size.Scale(1.2f);
  content_quad->SetSize(content_quad_size.width(), content_quad_size.height());
  AnimateBy(MsToDelta(0));

  EXPECT_CALL(*browser_, OnContentScreenBoundsChanged(testing::_)).Times(0);

  manager_->OnProjMatrixChanged(kProjMatrix);
}

TEST_F(UiSceneManagerTest, ApplyParentOpacity) {
  MakeManager(kNotInCct, kNotInWebVr);
  FakeUiElementRenderer renderer;
  auto grid_element = base::MakeUnique<Grid>();
  grid_element->set_draw_phase(kPhaseForeground);
  Grid* grid = grid_element.get();
  scene_->AddUiElement(k2dBrowsingContentGroup, std::move(grid_element));
  auto rect_element = base::MakeUnique<Rect>();
  rect_element->set_draw_phase(kPhaseForeground);
  Rect* rect = rect_element.get();
  scene_->AddUiElement(k2dBrowsingContentGroup, std::move(rect_element));

  ContentElement* content_quad =
      static_cast<ContentElement*>(scene_->GetUiElementByName(kContentQuad));
  content_quad->set_texture_id(1);
  TexturedElement* textured_element =
      static_cast<TexturedElement*>(scene_->GetUiElementByName(kExitPrompt));
  textured_element->SetVisible(true);
  textured_element->SetInitializedForTesting();

  AnimateBy(MsToDelta(0));
  content_quad->Render(&renderer, kProjMatrix);
  EXPECT_FLOAT_EQ(renderer.texture_opacity(), content_quad->computed_opacity());
  textured_element->Render(&renderer, kProjMatrix);
  EXPECT_FLOAT_EQ(renderer.texture_opacity(),
                  textured_element->computed_opacity());
  grid->Render(&renderer, kProjMatrix);
  EXPECT_FLOAT_EQ(renderer.grid_opacity(), grid->computed_opacity());
  rect->Render(&renderer, kProjMatrix);
  EXPECT_FLOAT_EQ(renderer.grid_opacity(), rect->computed_opacity());

  // Change parent opacity.
  scene_->GetUiElementByName(k2dBrowsingContentGroup)->SetOpacity(0.2);
  AnimateBy(MsToDelta(0));
  content_quad->Render(&renderer, kProjMatrix);
  EXPECT_FLOAT_EQ(renderer.texture_opacity(), content_quad->computed_opacity());
  textured_element->Render(&renderer, kProjMatrix);
  EXPECT_FLOAT_EQ(renderer.texture_opacity(),
                  textured_element->computed_opacity());
  grid->Render(&renderer, kProjMatrix);
  EXPECT_FLOAT_EQ(renderer.grid_opacity(), grid->computed_opacity());
  rect->Render(&renderer, kProjMatrix);
  EXPECT_FLOAT_EQ(renderer.grid_opacity(), rect->computed_opacity());
}

}  // namespace vr
