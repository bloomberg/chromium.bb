// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_scene_manager.h"

#include "base/macros.h"
#include "base/numerics/ranges.h"
#include "chrome/browser/vr/color_scheme.h"
#include "chrome/browser/vr/elements/content_element.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/elements/ui_element_name.h"
#include "chrome/browser/vr/model/model.h"
#include "chrome/browser/vr/target_property.h"
#include "chrome/browser/vr/test/animation_utils.h"
#include "chrome/browser/vr/test/constants.h"
#include "chrome/browser/vr/test/mock_browser_interface.h"
#include "chrome/browser/vr/test/ui_scene_manager_test.h"
#include "chrome/browser/vr/ui_scene.h"
#include "chrome/browser/vr/ui_scene_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebGestureEvent.h"

namespace vr {

namespace {
const std::set<UiElementName> kFloorCeilingBackgroundElements = {
    kBackgroundFront, kBackgroundLeft,   kBackgroundBack, kBackgroundRight,
    kBackgroundTop,   kBackgroundBottom, kCeiling,        kFloor};
const std::set<UiElementName> kElementsVisibleInBrowsing = {
    kBackgroundFront, kBackgroundLeft, kBackgroundBack,
    kBackgroundRight, kBackgroundTop,  kBackgroundBottom,
    kCeiling,         kFloor,          kContentQuad,
    kBackplane,       kUrlBar,         kUnderDevelopmentNotice};
const std::set<UiElementName> kElementsVisibleWithExitPrompt = {
    kBackgroundFront, kBackgroundLeft,     kBackgroundBack, kBackgroundRight,
    kBackgroundTop,   kBackgroundBottom,   kCeiling,        kFloor,
    kExitPrompt,      kExitPromptBackplane};
const std::set<UiElementName> kHitTestableElements = {
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
    kVoiceSearchButton,
};
const std::set<UiElementName> kElementsVisibleWithExitWarning = {
    kScreenDimmer, kExitWarning,
};

static constexpr float kTolerance = 1e-5f;
static constexpr float kSmallDelaySeconds = 0.1f;

MATCHER_P2(SizeFsAreApproximatelyEqual, other, tolerance, "") {
  return base::IsApproximatelyEqual(arg.width(), other.width(), tolerance) &&
         base::IsApproximatelyEqual(arg.height(), other.height(), tolerance);
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
  MakeManager(kNotInCct, kNotInWebVr);
  EXPECT_FALSE(IsVisible(kExclusiveScreenToast));

  manager_->SetFullscreen(true);
  AnimateBy(MsToDelta(10));
  EXPECT_TRUE(IsVisible(kExclusiveScreenToast));
  AnimateBy(base::TimeDelta::FromSecondsD(kToastTimeoutSeconds));
  EXPECT_FALSE(IsVisible(kExclusiveScreenToast));

  manager_->SetWebVrMode(true, true);
  AnimateBy(MsToDelta(10));
  EXPECT_TRUE(IsVisible(kExclusiveScreenToastViewportAware));
  AnimateBy(base::TimeDelta::FromSecondsD(kToastTimeoutSeconds));
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
  SkColor initial_background = SK_ColorBLACK;
  GetBackgroundColor(&initial_background);
  manager_->SetFullscreen(true);

  // Make sure background has changed for fullscreen.
  SkColor fullscreen_background = SK_ColorBLACK;
  GetBackgroundColor(&fullscreen_background);
  EXPECT_NE(initial_background, fullscreen_background);

  manager_->SetIncognito(true);

  // Make sure background has changed for incognito.
  SkColor incognito_background = SK_ColorBLACK;
  GetBackgroundColor(&incognito_background);
  EXPECT_NE(fullscreen_background, incognito_background);
  EXPECT_NE(initial_background, incognito_background);

  manager_->SetIncognito(false);
  SkColor no_longer_incognito_background = SK_ColorBLACK;
  GetBackgroundColor(&no_longer_incognito_background);
  EXPECT_EQ(fullscreen_background, no_longer_incognito_background);

  manager_->SetFullscreen(false);
  SkColor no_longer_fullscreen_background = SK_ColorBLACK;
  GetBackgroundColor(&no_longer_fullscreen_background);
  EXPECT_EQ(initial_background, no_longer_fullscreen_background);

  manager_->SetIncognito(true);
  SkColor incognito_again_background = SK_ColorBLACK;
  GetBackgroundColor(&incognito_again_background);
  EXPECT_EQ(incognito_background, incognito_again_background);

  manager_->SetIncognito(false);
  SkColor no_longer_incognito_again_background = SK_ColorBLACK;
  GetBackgroundColor(&no_longer_incognito_again_background);
  EXPECT_EQ(initial_background, no_longer_incognito_again_background);
}

TEST_F(UiSceneManagerTest, WebVrAutopresented) {
  MakeAutoPresentedManager();

  // Initially, we should only show the splash screen.
  auto initial_elements = std::set<UiElementName>();
  initial_elements.insert(kSplashScreenText);
  initial_elements.insert(kSplashScreenBackground);
  VerifyElementsVisible("Initial", initial_elements);

  // Enter WebVR with autopresentation.
  manager_->SetWebVrMode(true, false);
  manager_->OnWebVrFrameAvailable();
  // The splash screen should go away.
  AnimateBy(base::TimeDelta::FromSecondsD(kSplashScreenMinDurationSeconds +
                                          kSmallDelaySeconds));
  VerifyElementsVisible("Autopresented",
                        std::set<UiElementName>{kWebVrUrlToast});

  // Make sure the transient URL bar times out.
  AnimateBy(base::TimeDelta::FromSeconds(kWebVrUrlToastTimeoutSeconds +
                                         kSmallDelaySeconds));
  UiElement* transient_url_bar =
      scene_->GetUiElementByName(kWebVrUrlToastTransientParent);
  EXPECT_TRUE(IsAnimating(transient_url_bar, {OPACITY}));
  // Finish the transition.
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
  SkColor initial_background = SK_ColorBLACK;
  GetBackgroundColor(&initial_background);
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
  SkColor fullscreen_background = SK_ColorBLACK;
  GetBackgroundColor(&fullscreen_background);
  EXPECT_NE(initial_background, fullscreen_background);
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
  SkColor no_longer_fullscreen_background = SK_ColorBLACK;
  GetBackgroundColor(&no_longer_fullscreen_background);
  EXPECT_EQ(initial_background, no_longer_fullscreen_background);
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

  // Initial state.
  VerifyElementsVisible("Initial", kElementsVisibleInBrowsing);

  // Showing exit VR prompt should make prompt visible.
  manager_->SetExitVrPromptEnabled(true, UiUnsupportedMode::kUnhandledPageInfo);
  VerifyElementsVisible("Prompt visible", kElementsVisibleWithExitPrompt);
}

TEST_F(UiSceneManagerTest, UiUpdatesForHidingExitPrompt) {
  MakeManager(kNotInCct, kNotInWebVr);

  // Initial state.
  manager_->SetExitVrPromptEnabled(true, UiUnsupportedMode::kUnhandledPageInfo);
  VerifyElementsVisible("Initial", kElementsVisibleWithExitPrompt);

  // Hiding exit VR prompt should make prompt invisible.
  manager_->SetExitVrPromptEnabled(false, UiUnsupportedMode::kCount);
  VerifyElementsVisible("Prompt invisible", kElementsVisibleInBrowsing);
}

TEST_F(UiSceneManagerTest, BackplaneClickTriggersOnExitPrompt) {
  MakeManager(kNotInCct, kNotInWebVr);

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

  manager_->SetAudioCapturingIndicator(true);
  manager_->SetVideoCapturingIndicator(true);
  manager_->SetScreenCapturingIndicator(true);
  manager_->SetLocationAccessIndicator(true);
  manager_->SetBluetoothConnectedIndicator(true);

  auto* web_vr_root = scene_->GetUiElementByName(kWebVrRoot);
  for (auto& element : *web_vr_root) {
    SCOPED_TRACE(element.name());
    EXPECT_TRUE(element.draw_phase() == kPhaseNone ||
                element.draw_phase() == kPhaseOverlayBackground ||
                element.draw_phase() == kPhaseOverlayForeground);
  }

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

  manager_->OnProjMatrixChanged(kPixelDaydreamProjMatrix);
}

TEST_F(UiSceneManagerTest, PropagateContentBoundsOnFullscreen) {
  MakeManager(kNotInCct, kNotInWebVr);

  AnimateBy(MsToDelta(0));
  manager_->OnProjMatrixChanged(kPixelDaydreamProjMatrix);
  manager_->SetFullscreen(true);
  AnimateBy(MsToDelta(0));

  gfx::SizeF expected_bounds(0.587874f, 0.330614f);
  EXPECT_CALL(*browser_,
              OnContentScreenBoundsChanged(
                  SizeFsAreApproximatelyEqual(expected_bounds, kTolerance)));

  manager_->OnProjMatrixChanged(kPixelDaydreamProjMatrix);
}

TEST_F(UiSceneManagerTest, HitTestableElements) {
  MakeManager(kNotInCct, kNotInWebVr);
  AnimateBy(MsToDelta(0));
  CheckHitTestableRecursive(&scene_->root_element());
}

TEST_F(UiSceneManagerTest, DontPropagateContentBoundsOnNegligibleChange) {
  MakeManager(kNotInCct, kNotInWebVr);

  AnimateBy(MsToDelta(0));
  manager_->OnProjMatrixChanged(kPixelDaydreamProjMatrix);

  UiElement* content_quad = scene_->GetUiElementByName(kContentQuad);
  gfx::SizeF content_quad_size = content_quad->size();
  content_quad_size.Scale(1.2f);
  content_quad->SetSize(content_quad_size.width(), content_quad_size.height());
  AnimateBy(MsToDelta(0));

  EXPECT_CALL(*browser_, OnContentScreenBoundsChanged(testing::_)).Times(0);

  manager_->OnProjMatrixChanged(kPixelDaydreamProjMatrix);
}

TEST_F(UiSceneManagerTest, RendererUsesCorrectOpacity) {
  MakeManager(kNotInCct, kNotInWebVr);

  ContentElement* content_element =
      static_cast<ContentElement*>(scene_->GetUiElementByName(kContentQuad));
  content_element->SetTexture(0, vr::UiElementRenderer::kTextureLocationLocal);
  TexturedElement::SetInitializedForTesting();

  CheckRendererOpacityRecursive(&scene_->root_element());
}

TEST_F(UiSceneManagerTest, LoadingIndicatorBindings) {
  MakeManager(kNotInCct, kNotInWebVr);
  model_->loading = true;
  model_->load_progress = 0.5f;
  AnimateBy(MsToDelta(200));
  EXPECT_TRUE(VerifyVisibility({kLoadingIndicator}, true));
  UiElement* loading_indicator = scene_->GetUiElementByName(kLoadingIndicator);
  UiElement* loading_indicator_fg = loading_indicator->children().back().get();
  EXPECT_FLOAT_EQ(loading_indicator->size().width() * 0.5f,
                  loading_indicator_fg->size().width());
  float tx =
      loading_indicator_fg->GetTargetTransform().Apply().matrix().get(0, 3);
  EXPECT_FLOAT_EQ(loading_indicator->size().width() * 0.25f, tx);
}

TEST_F(UiSceneManagerTest, ExitWarning) {
  MakeManager(kNotInCct, kNotInWebVr);
  manager_->SetIsExiting();
  AnimateBy(MsToDelta(0));
  EXPECT_TRUE(VerifyVisibility(kElementsVisibleWithExitWarning, true));
}

// This test ensures that we maintain a specific view hierarchy so that our UI
// is not distorted based on a device's physical screen size. This test ensures
// that we don't silently cause distoration by changing the hierarchy. The long
// term solution is tracked by crbug.com/766318.
TEST_F(UiSceneManagerTest, EnforceSceneHierarchyForProjMatrixChanges) {
  MakeManager(kNotInCct, kNotInWebVr);
  UiElement* browsing_content_group =
      scene_->GetUiElementByName(k2dBrowsingContentGroup);
  UiElement* browsing_foreground =
      scene_->GetUiElementByName(k2dBrowsingForeground);
  UiElement* browsing_root = scene_->GetUiElementByName(k2dBrowsingRoot);
  UiElement* root = scene_->GetUiElementByName(kRoot);
  EXPECT_EQ(browsing_content_group->parent(), browsing_foreground);
  EXPECT_EQ(browsing_foreground->parent(), browsing_root);
  EXPECT_EQ(browsing_root->parent(), root);
  EXPECT_EQ(root->parent(), nullptr);
  // Parents of k2dBrowsingContentGroup should not animate transform. Note that
  // this test is not perfect because these could be animated anytime, but its
  // better than having no test.
  EXPECT_FALSE(
      browsing_foreground->IsAnimatingProperty(TargetProperty::TRANSFORM));
  EXPECT_FALSE(browsing_root->IsAnimatingProperty(TargetProperty::TRANSFORM));
  EXPECT_FALSE(root->IsAnimatingProperty(TargetProperty::TRANSFORM));
}

}  // namespace vr
