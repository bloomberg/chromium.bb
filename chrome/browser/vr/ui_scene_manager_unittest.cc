// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_scene_manager.h"

#include "base/macros.h"
#include "base/numerics/ranges.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/vr/elements/button.h"
#include "chrome/browser/vr/elements/content_element.h"
#include "chrome/browser/vr/elements/exit_prompt.h"
#include "chrome/browser/vr/elements/rect.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/elements/ui_element_name.h"
#include "chrome/browser/vr/elements/vector_icon.h"
#include "chrome/browser/vr/model/model.h"
#include "chrome/browser/vr/speech_recognizer.h"
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
    kAudioPermissionPrompt,
    kAudioPermissionPromptBackplane,
    kUrlBar,
    kLoadingIndicator,
    kWebVrTimeoutSpinner,
    kWebVrTimeoutMessage,
    kWebVrTimeoutMessageIcon,
    kWebVrTimeoutMessageText,
    kWebVrTimeoutMessageButtonText,
    kSpeechRecognitionResultBackplane,
};
const std::set<UiElementName> kSpecialHitTestableElements = {
    kCloseButton, kWebVrTimeoutMessageButton, kVoiceSearchButton,
    kSpeechRecognitionListeningCloseButton,
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
  // This shouldn't be necessary in the future once crbug.com/782395 is fixed.
  // we can use class name to identify a child element in a composited element
  // such as Button.
  if (kSpecialHitTestableElements.find(element->name()) !=
      kSpecialHitTestableElements.end()) {
    bool has_hittestable_child = false;
    for (auto& child : *element) {
      if (child.hit_testable())
        has_hittestable_child = true;
    }
    EXPECT_TRUE(has_hittestable_child)
        << "element name: " << UiElementNameToString(element->name());
    return;
  }
  const bool should_be_hit_testable =
      kHitTestableElements.find(element->name()) != kHitTestableElements.end();
  EXPECT_EQ(should_be_hit_testable, element->hit_testable())
      << "element name: " << UiElementNameToString(element->name());
  for (const auto& child : element->children()) {
    CheckHitTestableRecursive(child.get());
  }
}

void VerifyButtonColor(Button* button,
                       SkColor foreground_color,
                       SkColor background_color,
                       const std::string& trace_name) {
  SCOPED_TRACE(trace_name);
  EXPECT_EQ(button->foreground()->GetColor(), foreground_color);
  EXPECT_EQ(button->background()->edge_color(), background_color);
  EXPECT_EQ(button->background()->center_color(), background_color);
}

}  // namespace

TEST_F(UiSceneManagerTest, ToastStateTransitions) {
  // Tests toast not showing when directly entering VR though WebVR
  // presentation.
  MakeManager(kNotInCct, kInWebVr);
  EXPECT_FALSE(IsVisible(kExclusiveScreenToast));
  EXPECT_FALSE(IsVisible(kExclusiveScreenToastViewportAware));

  MakeManager(kNotInCct, kNotInWebVr);
  EXPECT_FALSE(IsVisible(kExclusiveScreenToast));
  EXPECT_FALSE(IsVisible(kExclusiveScreenToastViewportAware));

  ui_->SetFullscreen(true);
  EXPECT_TRUE(IsVisible(kExclusiveScreenToast));
  EXPECT_FALSE(IsVisible(kExclusiveScreenToastViewportAware));

  ui_->SetWebVrMode(true, true);
  ui_->OnWebVrFrameAvailable();
  EXPECT_FALSE(IsVisible(kExclusiveScreenToast));
  EXPECT_TRUE(IsVisible(kExclusiveScreenToastViewportAware));

  ui_->SetWebVrMode(false, false);
  // TODO(crbug.com/787582): we should not show the fullscreen toast again when
  // returning to fullscreen mode after presenting webvr.
  EXPECT_TRUE(IsVisible(kExclusiveScreenToast));
  EXPECT_FALSE(IsVisible(kExclusiveScreenToastViewportAware));

  ui_->SetFullscreen(false);
  EXPECT_FALSE(IsVisible(kExclusiveScreenToast));
  EXPECT_FALSE(IsVisible(kExclusiveScreenToastViewportAware));

  ui_->SetWebVrMode(true, false);
  EXPECT_FALSE(IsVisible(kExclusiveScreenToast));
  EXPECT_FALSE(IsVisible(kExclusiveScreenToastViewportAware));

  ui_->SetWebVrMode(false, false);
  EXPECT_FALSE(IsVisible(kExclusiveScreenToast));
  EXPECT_FALSE(IsVisible(kExclusiveScreenToastViewportAware));
}

TEST_F(UiSceneManagerTest, ToastTransience) {
  MakeManager(kNotInCct, kNotInWebVr);
  EXPECT_FALSE(IsVisible(kExclusiveScreenToast));

  ui_->SetFullscreen(true);
  EXPECT_TRUE(AnimateBy(MsToDelta(10)));
  EXPECT_TRUE(IsVisible(kExclusiveScreenToast));
  EXPECT_TRUE(AnimateBy(base::TimeDelta::FromSecondsD(kToastTimeoutSeconds)));
  EXPECT_FALSE(IsVisible(kExclusiveScreenToast));

  ui_->SetWebVrMode(true, true);
  ui_->OnWebVrFrameAvailable();
  EXPECT_TRUE(AnimateBy(MsToDelta(10)));
  EXPECT_TRUE(IsVisible(kExclusiveScreenToastViewportAware));
  EXPECT_TRUE(AnimateBy(base::TimeDelta::FromSecondsD(kToastTimeoutSeconds)));
  EXPECT_FALSE(IsVisible(kExclusiveScreenToastViewportAware));

  ui_->SetWebVrMode(false, false);
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
  ui_->SetFullscreen(true);
  EXPECT_TRUE(IsVisible(kCloseButton));
  ui_->SetFullscreen(false);
  EXPECT_FALSE(IsVisible(kCloseButton));

  // Button should not be visible when in WebVR.
  MakeManager(kInCct, kInWebVr);
  EXPECT_FALSE(IsVisible(kCloseButton));
  ui_->SetWebVrMode(false, false);
  EXPECT_TRUE(IsVisible(kCloseButton));

  // Button should be visible in Cct across transistions in fullscreen.
  MakeManager(kInCct, kNotInWebVr);
  EXPECT_TRUE(IsVisible(kCloseButton));
  ui_->SetFullscreen(true);
  EXPECT_TRUE(IsVisible(kCloseButton));
  ui_->SetFullscreen(false);
  EXPECT_TRUE(IsVisible(kCloseButton));
}

TEST_F(UiSceneManagerTest, UiUpdatesForIncognito) {
  MakeManager(kNotInCct, kNotInWebVr);

  // Hold onto the background color to make sure it changes.
  SkColor initial_background = SK_ColorBLACK;
  GetBackgroundColor(&initial_background);
  EXPECT_EQ(
      ColorScheme::GetColorScheme(ColorScheme::kModeNormal).world_background,
      initial_background);
  ui_->SetFullscreen(true);

  // Make sure background has changed for fullscreen.
  SkColor fullscreen_background = SK_ColorBLACK;
  GetBackgroundColor(&fullscreen_background);
  EXPECT_EQ(ColorScheme::GetColorScheme(ColorScheme::kModeFullscreen)
                .world_background,
            fullscreen_background);

  model_->incognito = true;
  // Make sure background has changed for incognito.
  SkColor incognito_background = SK_ColorBLACK;
  GetBackgroundColor(&incognito_background);
  EXPECT_EQ(
      ColorScheme::GetColorScheme(ColorScheme::kModeIncognito).world_background,
      incognito_background);

  model_->incognito = false;
  SkColor no_longer_incognito_background = SK_ColorBLACK;
  GetBackgroundColor(&no_longer_incognito_background);
  EXPECT_EQ(fullscreen_background, no_longer_incognito_background);

  ui_->SetFullscreen(false);
  SkColor no_longer_fullscreen_background = SK_ColorBLACK;
  GetBackgroundColor(&no_longer_fullscreen_background);
  EXPECT_EQ(initial_background, no_longer_fullscreen_background);

  model_->incognito = true;
  SkColor incognito_again_background = SK_ColorBLACK;
  GetBackgroundColor(&incognito_again_background);
  EXPECT_EQ(incognito_background, incognito_again_background);

  model_->incognito = false;
  SkColor no_longer_incognito_again_background = SK_ColorBLACK;
  GetBackgroundColor(&no_longer_incognito_again_background);
  EXPECT_EQ(initial_background, no_longer_incognito_again_background);
}

TEST_F(UiSceneManagerTest, VoiceSearchHiddenInIncognito) {
  MakeManager(kNotInCct, kNotInWebVr);

  EXPECT_TRUE(OnBeginFrame());
  EXPECT_TRUE(IsVisible(kVoiceSearchButton));

  model_->incognito = true;
  EXPECT_TRUE(OnBeginFrame());
  EXPECT_FALSE(IsVisible(kVoiceSearchButton));
}

TEST_F(UiSceneManagerTest, VoiceSearchHiddenWhenCantAskForPermission) {
  MakeManager(kNotInCct, kNotInWebVr);

  model_->speech.has_or_can_request_audio_permission = true;
  EXPECT_TRUE(OnBeginFrame());
  EXPECT_TRUE(IsVisible(kVoiceSearchButton));

  model_->speech.has_or_can_request_audio_permission = false;
  EXPECT_TRUE(OnBeginFrame());
  EXPECT_FALSE(IsVisible(kVoiceSearchButton));
}

TEST_F(UiSceneManagerTest, WebVrAutopresented) {
  MakeAutoPresentedManager();

  // Initially, we should only show the splash screen.
  auto initial_elements = std::set<UiElementName>();
  initial_elements.insert(kSplashScreenText);
  initial_elements.insert(kSplashScreenBackground);
  AnimateBy(MsToDelta(1));
  VerifyElementsVisible("Initial", initial_elements);

  // Enter WebVR with autopresentation.
  ui_->SetWebVrMode(true, false);
  model_->web_vr_timeout_state = kWebVrNoTimeoutPending;
  // The splash screen should go away.
  AnimateBy(
      MsToDelta(1000 * (kSplashScreenMinDurationSeconds + kSmallDelaySeconds)));
  VerifyElementsVisible("Autopresented",
                        std::set<UiElementName>{kWebVrUrlToast});

  // Make sure the transient URL bar times out.
  AnimateBy(
      MsToDelta(1000 * (kWebVrUrlToastTimeoutSeconds + kSmallDelaySeconds)));
  UiElement* transient_url_bar =
      scene_->GetUiElementByName(kWebVrUrlToastTransientParent);
  EXPECT_TRUE(IsAnimating(transient_url_bar, {OPACITY}));
  // Finish the transition.
  EXPECT_TRUE(AnimateBy(MsToDelta(1000)));
  EXPECT_FALSE(IsAnimating(transient_url_bar, {OPACITY}));
  EXPECT_FALSE(IsVisible(kWebVrUrlToast));
}

TEST_F(UiSceneManagerTest, AppButtonClickForAutopresentation) {
  MakeAutoPresentedManager();

  // Clicking app button should be a no-op.
  EXPECT_CALL(*browser_, ExitPresent()).Times(0);
  EXPECT_CALL(*browser_, ExitFullscreen()).Times(0);
  ui_->OnAppButtonClicked();
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
  ui_->SetFullscreen(true);
  VerifyElementsVisible("In fullscreen", visible_in_fullscreen);
  // Make sure background has changed for fullscreen.
  SkColor fullscreen_background = SK_ColorBLACK;
  GetBackgroundColor(&fullscreen_background);
  EXPECT_EQ(ColorScheme::GetColorScheme(ColorScheme::kModeFullscreen)
                .world_background,
            fullscreen_background);
  // Should have started transition.
  EXPECT_TRUE(IsAnimating(content_quad, {BOUNDS}));
  EXPECT_TRUE(IsAnimating(content_group, {TRANSFORM}));
  // Finish the transition.
  EXPECT_TRUE(AnimateBy(MsToDelta(1000)));
  EXPECT_FALSE(IsAnimating(content_quad, {BOUNDS}));
  EXPECT_FALSE(IsAnimating(content_group, {TRANSFORM}));
  EXPECT_NE(initial_content_size, content_quad->size());
  EXPECT_NE(initial_position, content_group->LocalTransform());

  // Everything should return to original state after leaving fullscreen.
  ui_->SetFullscreen(false);
  VerifyElementsVisible("Restore initial", kElementsVisibleInBrowsing);
  SkColor no_longer_fullscreen_background = SK_ColorBLACK;
  GetBackgroundColor(&no_longer_fullscreen_background);
  EXPECT_EQ(
      ColorScheme::GetColorScheme(ColorScheme::kModeNormal).world_background,
      no_longer_fullscreen_background);
  // Should have started transition.
  EXPECT_TRUE(IsAnimating(content_quad, {BOUNDS}));
  EXPECT_TRUE(IsAnimating(content_group, {TRANSFORM}));
  // Finish the transition.
  EXPECT_TRUE(AnimateBy(MsToDelta(1000)));
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
  browser_->OnUnsupportedMode(UiUnsupportedMode::kUnhandledPageInfo);
  VerifyElementsVisible("Prompt invisible", kElementsVisibleInBrowsing);
}

TEST_F(UiSceneManagerTest, UiUpdatesForShowingExitPrompt) {
  MakeManager(kNotInCct, kNotInWebVr);

  // Initial state.
  VerifyElementsVisible("Initial", kElementsVisibleInBrowsing);

  // Showing exit VR prompt should make prompt visible.
  model_->active_modal_prompt_type = kModalPromptTypeExitVRForSiteInfo;
  VerifyElementsVisible("Prompt visible", kElementsVisibleWithExitPrompt);
}

TEST_F(UiSceneManagerTest, UiUpdatesForHidingExitPrompt) {
  MakeManager(kNotInCct, kNotInWebVr);

  // Initial state.
  model_->active_modal_prompt_type = kModalPromptTypeExitVRForSiteInfo;
  VerifyElementsVisible("Initial", kElementsVisibleWithExitPrompt);

  // Hiding exit VR prompt should make prompt invisible.
  model_->active_modal_prompt_type = kModalPromptTypeNone;
  EXPECT_TRUE(AnimateBy(MsToDelta(1000)));
  VerifyElementsVisible("Prompt invisible", kElementsVisibleInBrowsing);
}

TEST_F(UiSceneManagerTest, BackplaneClickTriggersOnExitPrompt) {
  MakeManager(kNotInCct, kNotInWebVr);

  // Initial state.
  VerifyElementsVisible("Initial", kElementsVisibleInBrowsing);
  model_->active_modal_prompt_type = kModalPromptTypeExitVRForSiteInfo;

  // Click on backplane should trigger UI browser interface but not close
  // prompt.
  EXPECT_CALL(*browser_,
              OnExitVrPromptResult(ExitVrPromptChoice::CHOICE_NONE,
                                   UiUnsupportedMode::kUnhandledPageInfo));
  scene_->GetUiElementByName(kExitPromptBackplane)->OnButtonUp(gfx::PointF());
  VerifyElementsVisible("Prompt still visible", kElementsVisibleWithExitPrompt);
}

TEST_F(UiSceneManagerTest, PrimaryButtonClickTriggersOnExitPrompt) {
  MakeManager(kNotInCct, kNotInWebVr);

  // Initial state.
  VerifyElementsVisible("Initial", kElementsVisibleInBrowsing);
  model_->active_modal_prompt_type = kModalPromptTypeExitVRForSiteInfo;
  OnBeginFrame();

  // Click on 'OK' should trigger UI browser interface but not close prompt.
  EXPECT_CALL(*browser_,
              OnExitVrPromptResult(ExitVrPromptChoice::CHOICE_STAY,
                                   UiUnsupportedMode::kUnhandledPageInfo));
  static_cast<ExitPrompt*>(scene_->GetUiElementByName(kExitPrompt))
      ->ClickPrimaryButtonForTesting();
  VerifyElementsVisible("Prompt still visible", kElementsVisibleWithExitPrompt);
}

TEST_F(UiSceneManagerTest, SecondaryButtonClickTriggersOnExitPrompt) {
  MakeManager(kNotInCct, kNotInWebVr);

  // Initial state.
  VerifyElementsVisible("Initial", kElementsVisibleInBrowsing);
  model_->active_modal_prompt_type = kModalPromptTypeExitVRForSiteInfo;
  OnBeginFrame();

  // Click on 'Exit VR' should trigger UI browser interface but not close
  // prompt.
  EXPECT_CALL(*browser_,
              OnExitVrPromptResult(ExitVrPromptChoice::CHOICE_EXIT,
                                   UiUnsupportedMode::kUnhandledPageInfo));

  static_cast<ExitPrompt*>(scene_->GetUiElementByName(kExitPrompt))
      ->ClickSecondaryButtonForTesting();
  VerifyElementsVisible("Prompt still visible", kElementsVisibleWithExitPrompt);
}

TEST_F(UiSceneManagerTest, UiUpdatesForWebVR) {
  MakeManager(kNotInCct, kInWebVr);

  model_->permissions.audio_capture_enabled = true;
  model_->permissions.video_capture_enabled = true;
  model_->permissions.screen_capture_enabled = true;
  model_->permissions.location_access = true;
  model_->permissions.bluetooth_connected = true;

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
  model_->permissions.audio_capture_enabled = true;
  model_->permissions.video_capture_enabled = true;
  model_->permissions.screen_capture_enabled = true;
  model_->permissions.location_access = true;
  model_->permissions.bluetooth_connected = true;

  // Transition to WebVR mode
  ui_->SetWebVrMode(true, false);

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

  model_->permissions.audio_capture_enabled = true;
  model_->permissions.video_capture_enabled = true;
  model_->permissions.screen_capture_enabled = true;
  model_->permissions.location_access = true;
  model_->permissions.bluetooth_connected = true;
  EXPECT_TRUE(VerifyVisibility(indicators, true));
  EXPECT_TRUE(VerifyRequiresLayout(indicators, true));

  // Go into non-browser modes and make sure all indicators are hidden.
  ui_->SetWebVrMode(true, false);
  EXPECT_TRUE(VerifyVisibility(indicators, false));
  ui_->SetWebVrMode(false, false);
  ui_->SetFullscreen(true);
  EXPECT_TRUE(VerifyVisibility(indicators, false));
  ui_->SetFullscreen(false);

  // Back to browser, make sure the indicators reappear.
  EXPECT_TRUE(AnimateBy(MsToDelta(500)));
  EXPECT_TRUE(VerifyVisibility(indicators, true));
  EXPECT_TRUE(VerifyRequiresLayout(indicators, true));

  // Ensure they can be turned off.
  model_->permissions.audio_capture_enabled = false;
  model_->permissions.video_capture_enabled = false;
  model_->permissions.screen_capture_enabled = false;
  model_->permissions.location_access = false;
  model_->permissions.bluetooth_connected = false;
  EXPECT_TRUE(VerifyRequiresLayout(indicators, false));
}

TEST_F(UiSceneManagerTest, PropagateContentBoundsOnStart) {
  MakeManager(kNotInCct, kNotInWebVr);

  gfx::SizeF expected_bounds(0.495922f, 0.330614f);
  EXPECT_CALL(*browser_,
              OnContentScreenBoundsChanged(
                  SizeFsAreApproximatelyEqual(expected_bounds, kTolerance)));

  ui_->OnProjMatrixChanged(kPixelDaydreamProjMatrix);
  EXPECT_TRUE(AnimateBy(MsToDelta(500)));
}

TEST_F(UiSceneManagerTest, PropagateContentBoundsOnFullscreen) {
  MakeManager(kNotInCct, kNotInWebVr);

  EXPECT_TRUE(AnimateBy(MsToDelta(0)));
  ui_->OnProjMatrixChanged(kPixelDaydreamProjMatrix);
  ui_->SetFullscreen(true);

  gfx::SizeF expected_bounds(0.587874f, 0.330614f);
  EXPECT_CALL(*browser_,
              OnContentScreenBoundsChanged(
                  SizeFsAreApproximatelyEqual(expected_bounds, kTolerance)));

  ui_->OnProjMatrixChanged(kPixelDaydreamProjMatrix);
  EXPECT_TRUE(AnimateBy(MsToDelta(500)));
}

TEST_F(UiSceneManagerTest, HitTestableElements) {
  MakeManager(kNotInCct, kNotInWebVr);
  EXPECT_TRUE(AnimateBy(MsToDelta(0)));
  CheckHitTestableRecursive(&scene_->root_element());
}

TEST_F(UiSceneManagerTest, DontPropagateContentBoundsOnNegligibleChange) {
  MakeManager(kNotInCct, kNotInWebVr);

  EXPECT_TRUE(AnimateBy(MsToDelta(0)));
  ui_->OnProjMatrixChanged(kPixelDaydreamProjMatrix);

  UiElement* content_quad = scene_->GetUiElementByName(kContentQuad);
  gfx::SizeF content_quad_size = content_quad->size();
  content_quad_size.Scale(1.2f);
  content_quad->SetSize(content_quad_size.width(), content_quad_size.height());
  EXPECT_TRUE(AnimateBy(MsToDelta(0)));

  EXPECT_CALL(*browser_, OnContentScreenBoundsChanged(testing::_)).Times(0);

  ui_->OnProjMatrixChanged(kPixelDaydreamProjMatrix);
}

TEST_F(UiSceneManagerTest, RendererUsesCorrectOpacity) {
  MakeManager(kNotInCct, kNotInWebVr);

  ContentElement* content_element =
      static_cast<ContentElement*>(scene_->GetUiElementByName(kContentQuad));
  content_element->SetTextureId(0);
  content_element->SetTextureLocation(UiElementRenderer::kTextureLocationLocal);
  TexturedElement::SetInitializedForTesting();

  CheckRendererOpacityRecursive(&scene_->root_element());
}

TEST_F(UiSceneManagerTest, LoadingIndicatorBindings) {
  MakeManager(kNotInCct, kNotInWebVr);
  model_->loading = true;
  model_->load_progress = 0.5f;
  EXPECT_TRUE(AnimateBy(MsToDelta(200)));
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
  ui_->SetIsExiting();
  EXPECT_TRUE(AnimateBy(MsToDelta(0)));
  EXPECT_TRUE(VerifyVisibility(kElementsVisibleWithExitWarning, true));
}

TEST_F(UiSceneManagerTest, WebVrTimeout) {
  MakeManager(kNotInCct, kInWebVr);

  ui_->SetWebVrMode(true, false);
  model_->web_vr_timeout_state = kWebVrAwaitingFirstFrame;

  AnimateBy(MsToDelta(500));
  VerifyVisibility(
      {
          kWebVrTimeoutSpinner, kWebVrTimeoutMessage,
          kWebVrTimeoutMessageLayout, kWebVrTimeoutMessageIcon,
          kWebVrTimeoutMessageText, kWebVrTimeoutMessageButton,
          kWebVrTimeoutMessageButtonText,
      },
      false);
  VerifyVisibility(
      {
          kWebVrTimeoutSpinnerBackground,
      },
      true);

  model_->web_vr_timeout_state = kWebVrTimeoutImminent;
  AnimateBy(MsToDelta(500));
  VerifyVisibility(
      {
          kWebVrTimeoutMessage, kWebVrTimeoutMessageLayout,
          kWebVrTimeoutMessageIcon, kWebVrTimeoutMessageText,
          kWebVrTimeoutMessageButton, kWebVrTimeoutMessageButtonText,
      },
      false);
  VerifyVisibility(
      {
          kWebVrTimeoutSpinner, kWebVrTimeoutSpinnerBackground,
      },
      true);

  model_->web_vr_timeout_state = kWebVrTimedOut;
  AnimateBy(MsToDelta(500));
  VerifyVisibility(
      {
          kWebVrTimeoutSpinner,
      },
      false);
  VerifyVisibility(
      {
          kWebVrTimeoutSpinnerBackground, kWebVrTimeoutMessage,
          kWebVrTimeoutMessageLayout, kWebVrTimeoutMessageIcon,
          kWebVrTimeoutMessageText, kWebVrTimeoutMessageButton,
          kWebVrTimeoutMessageButtonText,
      },
      true);
}

TEST_F(UiSceneManagerTest, SpeechRecognitionUiVisibility) {
  MakeManager(kNotInCct, kNotInWebVr);

  model_->speech.recognizing_speech = true;

  // Start hiding browsing foreground and showing speech recognition listening
  // UI.
  EXPECT_TRUE(AnimateBy(MsToDelta(10)));
  VerifyIsAnimating({k2dBrowsingForeground, kSpeechRecognitionListening},
                    {OPACITY}, true);
  VerifyIsAnimating({kSpeechRecognitionResult}, {OPACITY}, false);

  EXPECT_TRUE(
      AnimateBy(MsToDelta(kSpeechRecognitionOpacityAnimationDurationMs)));
  // All opacity animations should be finished at this point.
  VerifyIsAnimating({k2dBrowsingForeground, kSpeechRecognitionListening,
                     kSpeechRecognitionResult},
                    {OPACITY}, false);
  VerifyIsAnimating({kSpeechRecognitionListeningGrowingCircle}, {CIRCLE_GROW},
                    false);
  VerifyVisibility(
      {kSpeechRecognitionListening, kSpeechRecognitionListeningMicrophoneIcon,
       kSpeechRecognitionListeningCloseButton,
       kSpeechRecognitionListeningInnerCircle,
       kSpeechRecognitionListeningGrowingCircle},
      true);
  VerifyVisibility({k2dBrowsingForeground, kSpeechRecognitionResult}, false);

  model_->speech.speech_recognition_state = SPEECH_RECOGNITION_READY;
  EXPECT_TRUE(AnimateBy(MsToDelta(10)));
  VerifyIsAnimating({kSpeechRecognitionListeningGrowingCircle}, {CIRCLE_GROW},
                    true);

  // Mock received speech result.
  model_->speech.recognition_result = base::ASCIIToUTF16("test");
  model_->speech.recognizing_speech = false;
  model_->speech.speech_recognition_state = SPEECH_RECOGNITION_END;

  EXPECT_TRUE(AnimateBy(MsToDelta(10)));
  VerifyVisibility({kSpeechRecognitionResult, kSpeechRecognitionResultCircle,
                    kSpeechRecognitionResultMicrophoneIcon,
                    kSpeechRecognitionResultBackplane},
                   true);
  // Speech result UI should show instantly while listening UI hide immediately.
  VerifyIsAnimating({k2dBrowsingForeground, kSpeechRecognitionListening,
                     kSpeechRecognitionResult},
                    {OPACITY}, false);
  VerifyIsAnimating({kSpeechRecognitionListeningGrowingCircle}, {CIRCLE_GROW},
                    false);
  VerifyVisibility({k2dBrowsingForeground, kSpeechRecognitionListening}, false);

  // The visibility of Speech Recognition UI should not change at this point.
  EXPECT_FALSE(AnimateBy(MsToDelta(10)));

  EXPECT_TRUE(
      AnimateBy(MsToDelta(kSpeechRecognitionResultTimeoutSeconds * 1000)));
  // Start hide speech recognition result and show browsing foreground.
  VerifyIsAnimating({k2dBrowsingForeground, kSpeechRecognitionResult},
                    {OPACITY}, true);
  VerifyIsAnimating({kSpeechRecognitionListening}, {OPACITY}, false);

  EXPECT_TRUE(
      AnimateBy(MsToDelta(kSpeechRecognitionOpacityAnimationDurationMs)));
  VerifyIsAnimating({k2dBrowsingForeground, kSpeechRecognitionListening,
                     kSpeechRecognitionResult},
                    {OPACITY}, false);

  // Visibility is as expected.
  VerifyVisibility({kSpeechRecognitionListening, kSpeechRecognitionResult},
                   false);
  EXPECT_TRUE(IsVisible(k2dBrowsingForeground));
}

TEST_F(UiSceneManagerTest, SpeechRecognitionUiVisibilityNoResult) {
  MakeManager(kNotInCct, kNotInWebVr);

  model_->speech.recognizing_speech = true;
  EXPECT_TRUE(
      AnimateBy(MsToDelta(kSpeechRecognitionOpacityAnimationDurationMs)));
  VerifyIsAnimating({k2dBrowsingForeground, kSpeechRecognitionListening},
                    {OPACITY}, false);

  // Mock exit without a recognition result
  model_->speech.recognition_result.clear();
  model_->speech.recognizing_speech = false;
  model_->speech.speech_recognition_state = SPEECH_RECOGNITION_END;

  EXPECT_TRUE(AnimateBy(MsToDelta(10)));
  VerifyIsAnimating({k2dBrowsingForeground, kSpeechRecognitionListening},
                    {OPACITY}, true);
  VerifyIsAnimating({kSpeechRecognitionResult}, {OPACITY}, false);

  EXPECT_TRUE(
      AnimateBy(MsToDelta(kSpeechRecognitionOpacityAnimationDurationMs)));
  VerifyIsAnimating({k2dBrowsingForeground, kSpeechRecognitionListening,
                     kSpeechRecognitionResult},
                    {OPACITY}, false);

  // Visibility is as expected.
  VerifyVisibility({kSpeechRecognitionListening, kSpeechRecognitionResult},
                   false);
  EXPECT_TRUE(IsVisible(k2dBrowsingForeground));
}

TEST_F(UiSceneManagerTest, OmniboxSuggestionBindings) {
  MakeManager(kNotInCct, kNotInWebVr);
  UiElement* container = scene_->GetUiElementByName(kSuggestionLayout);
  ASSERT_NE(container, nullptr);

  OnBeginFrame();
  EXPECT_EQ(container->children().size(), 0u);
  int initially_visible = NumVisibleChildren(kSuggestionLayout);

  model_->omnibox_suggestions.emplace_back(
      OmniboxSuggestion(base::string16(), base::string16(),
                        AutocompleteMatch::Type::VOICE_SUGGEST, GURL()));
  OnBeginFrame();
  EXPECT_EQ(container->children().size(), 1u);
  EXPECT_GT(NumVisibleChildren(kSuggestionLayout), initially_visible);

  model_->omnibox_suggestions.clear();
  OnBeginFrame();
  EXPECT_EQ(container->children().size(), 0u);
  EXPECT_EQ(NumVisibleChildren(kSuggestionLayout), initially_visible);
}

TEST_F(UiSceneManagerTest, OmniboxSuggestionNavigates) {
  MakeManager(kNotInCct, kNotInWebVr);
  GURL gurl("http://test.com/");
  model_->omnibox_suggestions.emplace_back(
      OmniboxSuggestion(base::string16(), base::string16(),
                        AutocompleteMatch::Type::VOICE_SUGGEST, gurl));
  OnBeginFrame();

  UiElement* suggestions = scene_->GetUiElementByName(kSuggestionLayout);
  ASSERT_NE(suggestions, nullptr);
  UiElement* suggestion = suggestions->children().front().get();
  ASSERT_NE(suggestion, nullptr);
  EXPECT_CALL(*browser_, Navigate(gurl)).Times(1);
  suggestion->OnButtonDown({0, 0});
  suggestion->OnButtonUp({0, 0});
}

TEST_F(UiSceneManagerTest, ControllerQuiescence) {
  MakeManager(kNotInCct, kNotInWebVr);
  OnBeginFrame();
  EXPECT_TRUE(IsVisible(kControllerGroup));
  model_->controller.quiescent = true;
  EXPECT_TRUE(AnimateBy(MsToDelta(500)));
  EXPECT_TRUE(IsVisible(kControllerGroup));
  EXPECT_TRUE(AnimateBy(MsToDelta(100)));
  EXPECT_FALSE(IsVisible(kControllerGroup));

  UiElement* controller_group = scene_->GetUiElementByName(kControllerGroup);
  model_->controller.quiescent = false;
  EXPECT_TRUE(AnimateBy(MsToDelta(100)));
  EXPECT_GT(1.0f, controller_group->computed_opacity());
  EXPECT_TRUE(AnimateBy(MsToDelta(150)));
  EXPECT_EQ(1.0f, controller_group->computed_opacity());
}

TEST_F(UiSceneManagerTest, CloseButtonColorBindings) {
  MakeManager(kInCct, kNotInWebVr);
  EXPECT_TRUE(IsVisible(kCloseButton));
  Button* button =
      static_cast<Button*>(scene_->GetUiElementByName(kCloseButton));
  for (int i = 0; i < ColorScheme::kNumModes; i++) {
    ColorScheme::Mode mode = static_cast<ColorScheme::Mode>(i);
    SCOPED_TRACE(mode);
    if (mode == ColorScheme::kModeIncognito) {
      ui_->SetIncognito(true);
    } else if (mode == ColorScheme::kModeFullscreen) {
      ui_->SetIncognito(false);
      ui_->SetFullscreen(true);
    }
    ColorScheme scheme = ColorScheme::GetColorScheme(mode);
    EXPECT_TRUE(OnBeginFrame());
    VerifyButtonColor(button, scheme.button_colors.foreground,
                      scheme.button_colors.background, "normal");
    button->hit_plane()->OnHoverEnter(gfx::PointF(0.5f, 0.5f));
    EXPECT_TRUE(OnBeginFrame());
    VerifyButtonColor(button, scheme.button_colors.foreground,
                      scheme.button_colors.background_hover, "hover");
    button->hit_plane()->OnButtonDown(gfx::PointF(0.5f, 0.5f));
    EXPECT_TRUE(OnBeginFrame());
    VerifyButtonColor(button, scheme.button_colors.foreground,
                      scheme.button_colors.background_down, "down");
    button->hit_plane()->OnMove(gfx::PointF());
    EXPECT_TRUE(OnBeginFrame());
    VerifyButtonColor(button, scheme.button_colors.foreground,
                      scheme.button_colors.background, "move");
    button->hit_plane()->OnButtonUp(gfx::PointF());
    EXPECT_TRUE(OnBeginFrame());
    VerifyButtonColor(button, scheme.button_colors.foreground,
                      scheme.button_colors.background, "up");
  }
}

}  // namespace vr
