// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_scene_creator.h"

#include "base/macros.h"
#include "base/numerics/ranges.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "chrome/browser/vr/elements/button.h"
#include "chrome/browser/vr/elements/content_element.h"
#include "chrome/browser/vr/elements/disc_button.h"
#include "chrome/browser/vr/elements/exit_prompt.h"
#include "chrome/browser/vr/elements/rect.h"
#include "chrome/browser/vr/elements/repositioner.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/elements/ui_element_name.h"
#include "chrome/browser/vr/elements/vector_icon.h"
#include "chrome/browser/vr/model/assets.h"
#include "chrome/browser/vr/model/model.h"
#include "chrome/browser/vr/speech_recognizer.h"
#include "chrome/browser/vr/target_property.h"
#include "chrome/browser/vr/test/animation_utils.h"
#include "chrome/browser/vr/test/constants.h"
#include "chrome/browser/vr/test/mock_ui_browser_interface.h"
#include "chrome/browser/vr/test/ui_test.h"
#include "chrome/browser/vr/ui_renderer.h"
#include "chrome/browser/vr/ui_scene.h"
#include "chrome/browser/vr/ui_scene_constants.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebGestureEvent.h"

namespace vr {

namespace {
const std::set<UiElementName> kFloorCeilingBackgroundElements = {
    kBackgroundFront, kBackgroundLeft,   kBackgroundBack, kBackgroundRight,
    kBackgroundTop,   kBackgroundBottom, kCeiling,        kFloor};
const std::set<UiElementName> kElementsVisibleInBrowsing = {
    kBackgroundFront,
    kBackgroundLeft,
    kBackgroundBack,
    kBackgroundRight,
    kBackgroundTop,
    kBackgroundBottom,
    kCeiling,
    kFloor,
    kContentFrame,
    kContentFrameHitPlane,
    kContentQuad,
    kContentQuadShadow,
    kBackplane,
    kUrlBarBackplane,
    kUrlBarBackButton,
    kUrlBarBackButtonIcon,
    kUrlBarSeparator,
    kUrlBarOriginRegion,
    kUrlBarOriginContent,
    kController,
    kReticle,
    kLaser,
    kControllerTouchpadButton,
    kControllerAppButton,
    kControllerHomeButton,
};
const std::set<UiElementName> kElementsVisibleWithExitPrompt = {
    kBackgroundFront,
    kBackgroundLeft,
    kBackgroundBack,
    kBackgroundRight,
    kBackgroundTop,
    kBackgroundBottom,
    kCeiling,
    kFloor,
    kExitPrompt,
    kExitPromptBackplane,
    kController,
    kReticle,
    kLaser,
    kControllerTouchpadButton,
    kControllerAppButton,
    kControllerHomeButton,
};
const std::set<UiElementName> kElementsVisibleWithExitWarning = {
    kScreenDimmer, kExitWarningBackground, kExitWarningText};
const std::set<UiElementName> kElementsVisibleWithVoiceSearch = {
    kSpeechRecognitionListening, kSpeechRecognitionMicrophoneIcon,
    kSpeechRecognitionListeningCloseButton, kSpeechRecognitionCircle,
    kSpeechRecognitionListeningGrowingCircle};
const std::set<UiElementName> kElementsVisibleWithVoiceSearchResult = {
    kSpeechRecognitionResult, kSpeechRecognitionCircle,
    kSpeechRecognitionMicrophoneIcon, kSpeechRecognitionResultBackplane};

static constexpr float kTolerance = 1e-5f;
static constexpr float kSmallDelaySeconds = 0.1f;

MATCHER_P2(SizeFsAreApproximatelyEqual, other, tolerance, "") {
  return base::IsApproximatelyEqual(arg.width(), other.width(), tolerance) &&
         base::IsApproximatelyEqual(arg.height(), other.height(), tolerance);
}

void VerifyButtonColor(DiscButton* button,
                       SkColor foreground_color,
                       SkColor background_color,
                       const std::string& trace_name) {
  SCOPED_TRACE(trace_name);
  EXPECT_EQ(button->foreground()->GetColor(), foreground_color);
  EXPECT_EQ(button->background()->edge_color(), background_color);
  EXPECT_EQ(button->background()->center_color(), background_color);
}

}  // namespace

TEST_F(UiTest, ToastStateTransitions) {
  // Tests toast not showing when directly entering VR though WebVR
  // presentation.
  CreateScene(kNotInCct, kInWebVr);
  EXPECT_FALSE(IsVisible(kExclusiveScreenToast));
  EXPECT_FALSE(IsVisible(kExclusiveScreenToastViewportAware));

  CreateScene(kNotInCct, kNotInWebVr);
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

TEST_F(UiTest, ToastTransience) {
  CreateScene(kNotInCct, kNotInWebVr);
  EXPECT_FALSE(IsVisible(kExclusiveScreenToast));

  ui_->SetFullscreen(true);
  EXPECT_TRUE(IsVisible(kExclusiveScreenToast));
  EXPECT_TRUE(RunFor(base::TimeDelta::FromSecondsD(kToastTimeoutSeconds +
                                                   kSmallDelaySeconds)));
  EXPECT_FALSE(IsVisible(kExclusiveScreenToast));

  ui_->SetWebVrMode(true, true);
  ui_->OnWebVrFrameAvailable();
  EXPECT_TRUE(IsVisible(kExclusiveScreenToastViewportAware));
  EXPECT_TRUE(RunFor(base::TimeDelta::FromSecondsD(kToastTimeoutSeconds +
                                                   kSmallDelaySeconds)));
  EXPECT_FALSE(IsVisible(kExclusiveScreenToastViewportAware));

  ui_->SetWebVrMode(false, false);
  EXPECT_FALSE(IsVisible(kExclusiveScreenToastViewportAware));
}

TEST_F(UiTest, CloseButtonVisibleInCctFullscreen) {
  // Button should be visible in cct.
  CreateScene(kInCct, kNotInWebVr);
  EXPECT_TRUE(IsVisible(kCloseButton));

  // Button should not be visible when not in cct or fullscreen.
  CreateScene(kNotInCct, kNotInWebVr);
  EXPECT_FALSE(IsVisible(kCloseButton));

  // Button should be visible in fullscreen and hidden when leaving fullscreen.
  ui_->SetFullscreen(true);
  EXPECT_TRUE(IsVisible(kCloseButton));
  ui_->SetFullscreen(false);
  EXPECT_FALSE(IsVisible(kCloseButton));

  // Button should not be visible when in WebVR.
  CreateScene(kInCct, kInWebVr);
  EXPECT_FALSE(IsVisible(kCloseButton));
  ui_->SetWebVrMode(false, false);
  EXPECT_TRUE(IsVisible(kCloseButton));

  // Button should be visible in Cct across transistions in fullscreen.
  CreateScene(kInCct, kNotInWebVr);
  EXPECT_TRUE(IsVisible(kCloseButton));
  ui_->SetFullscreen(true);
  EXPECT_TRUE(IsVisible(kCloseButton));
  ui_->SetFullscreen(false);
  EXPECT_TRUE(IsVisible(kCloseButton));
}

TEST_F(UiTest, UiUpdatesForIncognito) {
  CreateScene(kNotInCct, kNotInWebVr);

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

TEST_F(UiTest, VoiceSearchHiddenInIncognito) {
  CreateScene(kNotInCct, kNotInWebVr);

  model_->push_mode(kModeEditingOmnibox);
  EXPECT_TRUE(OnBeginFrame());
  EXPECT_TRUE(IsVisible(kOmniboxVoiceSearchButton));

  model_->incognito = true;
  EXPECT_TRUE(OnBeginFrame());
  EXPECT_FALSE(IsVisible(kOmniboxVoiceSearchButton));
}

TEST_F(UiTest, VoiceSearchHiddenWhenCantAskForPermission) {
  CreateScene(kNotInCct, kNotInWebVr);

  model_->push_mode(kModeEditingOmnibox);
  model_->speech.has_or_can_request_audio_permission = true;
  EXPECT_TRUE(OnBeginFrame());
  EXPECT_TRUE(IsVisible(kOmniboxVoiceSearchButton));

  model_->speech.has_or_can_request_audio_permission = false;
  EXPECT_TRUE(OnBeginFrame());
  EXPECT_FALSE(IsVisible(kOmniboxVoiceSearchButton));
}

TEST_F(UiTest, VoiceSearchHiddenWhenContentCapturingAudio) {
  CreateScene(kNotInCct, kNotInWebVr);

  model_->push_mode(kModeEditingOmnibox);
  model_->speech.has_or_can_request_audio_permission = true;
  model_->capturing_state.audio_capture_enabled = false;
  EXPECT_TRUE(OnBeginFrame());
  EXPECT_TRUE(IsVisible(kOmniboxVoiceSearchButton));

  model_->capturing_state.audio_capture_enabled = true;
  EXPECT_TRUE(OnBeginFrame());
  EXPECT_FALSE(IsVisible(kOmniboxVoiceSearchButton));
}

TEST_F(UiTest, UiModeWebVr) {
  CreateScene(kNotInCct, kNotInWebVr);

  EXPECT_EQ(model_->ui_modes.size(), 1u);
  EXPECT_EQ(model_->ui_modes.back(), kModeBrowsing);
  VerifyOnlyElementsVisible("Initial", kElementsVisibleInBrowsing);

  ui_->SetWebVrMode(true, false);
  EXPECT_EQ(model_->ui_modes.size(), 2u);
  EXPECT_EQ(model_->ui_modes[1], kModeWebVr);
  EXPECT_EQ(model_->ui_modes[0], kModeBrowsing);
  VerifyOnlyElementsVisible("WebVR", {kWebVrBackground});

  ui_->SetWebVrMode(false, false);
  EXPECT_EQ(model_->ui_modes.size(), 1u);
  EXPECT_EQ(model_->ui_modes.back(), kModeBrowsing);
  VerifyOnlyElementsVisible("Browsing after WebVR", kElementsVisibleInBrowsing);
}

TEST_F(UiTest, UiModeVoiceSearch) {
  CreateScene(kNotInCct, kNotInWebVr);

  EXPECT_EQ(model_->ui_modes.size(), 1u);
  EXPECT_EQ(model_->ui_modes.back(), kModeBrowsing);
  VerifyOnlyElementsVisible("Initial", kElementsVisibleInBrowsing);

  ui_->SetSpeechRecognitionEnabled(true);
  EXPECT_EQ(model_->ui_modes.size(), 2u);
  EXPECT_EQ(model_->ui_modes[1], kModeVoiceSearch);
  EXPECT_EQ(model_->ui_modes[0], kModeBrowsing);
  ui_->SetSpeechRecognitionEnabled(true);
  VerifyVisibility(kElementsVisibleWithVoiceSearch, true);

  ui_->SetSpeechRecognitionEnabled(false);
  EXPECT_EQ(model_->ui_modes.size(), 1u);
  EXPECT_EQ(model_->ui_modes.back(), kModeBrowsing);
  OnBeginFrame();
  OnBeginFrame();
  VerifyOnlyElementsVisible("Browsing", kElementsVisibleInBrowsing);
}

TEST_F(UiTest, UiModeOmniboxEditing) {
  CreateScene(kNotInCct, kNotInWebVr);

  EXPECT_EQ(model_->ui_modes.size(), 1u);
  EXPECT_EQ(model_->ui_modes.back(), kModeBrowsing);
  EXPECT_EQ(NumVisibleInTree(kOmniboxRoot), 0);
  VerifyOnlyElementsVisible("Initial", kElementsVisibleInBrowsing);

  model_->push_mode(kModeEditingOmnibox);
  OnBeginFrame();
  EXPECT_EQ(model_->ui_modes.size(), 2u);
  EXPECT_EQ(model_->ui_modes[1], kModeEditingOmnibox);
  EXPECT_EQ(model_->ui_modes[0], kModeBrowsing);
  EXPECT_GT(NumVisibleInTree(kOmniboxRoot), 0);

  model_->pop_mode(kModeEditingOmnibox);
  OnBeginFrame();
  EXPECT_EQ(model_->ui_modes.size(), 1u);
  EXPECT_EQ(model_->ui_modes.back(), kModeBrowsing);
  VerifyOnlyElementsVisible("Browsing", kElementsVisibleInBrowsing);
}

TEST_F(UiTest, UiModeVoiceSearchFromOmnibox) {
  CreateScene(kNotInCct, kNotInWebVr);

  EXPECT_EQ(model_->ui_modes.size(), 1u);
  EXPECT_EQ(model_->ui_modes.back(), kModeBrowsing);
  EXPECT_EQ(NumVisibleInTree(kOmniboxRoot), 0);
  VerifyOnlyElementsVisible("Initial", kElementsVisibleInBrowsing);

  model_->push_mode(kModeEditingOmnibox);
  OnBeginFrame();
  EXPECT_EQ(model_->ui_modes.size(), 2u);
  EXPECT_EQ(model_->ui_modes[1], kModeEditingOmnibox);
  EXPECT_EQ(model_->ui_modes[0], kModeBrowsing);
  EXPECT_GT(NumVisibleInTree(kOmniboxRoot), 0);

  ui_->SetSpeechRecognitionEnabled(true);
  EXPECT_EQ(model_->ui_modes.size(), 3u);
  EXPECT_EQ(model_->ui_modes[2], kModeVoiceSearch);
  EXPECT_EQ(model_->ui_modes[1], kModeEditingOmnibox);
  EXPECT_EQ(model_->ui_modes[0], kModeBrowsing);
  OnBeginFrame();
  EXPECT_EQ(NumVisibleInTree(kOmniboxRoot), 0);
  VerifyVisibility(kElementsVisibleWithVoiceSearch, true);

  ui_->SetSpeechRecognitionEnabled(false);
  EXPECT_EQ(model_->ui_modes.size(), 2u);
  EXPECT_EQ(model_->ui_modes[1], kModeEditingOmnibox);
  EXPECT_EQ(model_->ui_modes[0], kModeBrowsing);
  OnBeginFrame();
  OnBeginFrame();
  EXPECT_GT(NumVisibleInTree(kOmniboxRoot), 0);

  model_->pop_mode(kModeEditingOmnibox);
  EXPECT_EQ(model_->ui_modes.size(), 1u);
  EXPECT_EQ(model_->ui_modes.back(), kModeBrowsing);
  OnBeginFrame();
  VerifyOnlyElementsVisible("Browsing", kElementsVisibleInBrowsing);
}

TEST_F(UiTest, WebVrAutopresented) {
  CreateSceneForAutoPresentation();

  // Initially, we should only show the splash screen.
  VerifyOnlyElementsVisible("Initial", {kSplashScreenText, kWebVrBackground});

  // Enter WebVR with autopresentation.
  ui_->SetWebVrMode(true, false);

  // The splash screen should go away.
  RunFor(
      MsToDelta(1000 * (kSplashScreenMinDurationSeconds + kSmallDelaySeconds)));
  ui_->OnWebVrFrameAvailable();
  EXPECT_TRUE(RunFor(MsToDelta(10)));
  VerifyOnlyElementsVisible("Autopresented", {kWebVrUrlToast});

  // Make sure the transient URL bar times out.
  RunFor(MsToDelta(1000 * (kWebVrUrlToastTimeoutSeconds + kSmallDelaySeconds)));
  UiElement* transient_url_bar =
      scene_->GetUiElementByName(kWebVrUrlToastTransientParent);
  EXPECT_TRUE(IsAnimating(transient_url_bar, {OPACITY}));
  // Finish the transition.
  EXPECT_TRUE(RunFor(MsToDelta(1000)));
  EXPECT_FALSE(IsAnimating(transient_url_bar, {OPACITY}));
  EXPECT_FALSE(IsVisible(kWebVrUrlToast));
}

TEST_F(UiTest, WebVrSplashScreenHiddenWhenTimeoutImminent) {
  CreateSceneForAutoPresentation();

  // Initially, we should only show the splash screen.
  VerifyOnlyElementsVisible("Initial", {kSplashScreenText, kWebVrBackground});

  ui_->SetWebVrMode(true, false);
  EXPECT_TRUE(RunFor(MsToDelta(
      1000 * (kSplashScreenMinDurationSeconds + kSmallDelaySeconds * 2))));

  ui_->OnWebVrTimeoutImminent();
  EXPECT_TRUE(RunFor(MsToDelta(10)));

  VerifyOnlyElementsVisible("Timeout imminent",
                            {kWebVrTimeoutSpinner, kWebVrBackground});
}

TEST_F(UiTest, AppButtonClickForAutopresentation) {
  CreateSceneForAutoPresentation();

  // Clicking app button should be a no-op.
  EXPECT_CALL(*browser_, ExitPresent()).Times(0);
  EXPECT_CALL(*browser_, ExitFullscreen()).Times(0);
  ui_->OnAppButtonClicked();
}

TEST_F(UiTest, UiUpdatesForFullscreenChanges) {
  auto visible_in_fullscreen = kFloorCeilingBackgroundElements;
  visible_in_fullscreen.insert(kContentQuad);
  visible_in_fullscreen.insert(kContentQuadShadow);
  visible_in_fullscreen.insert(kBackplane);
  visible_in_fullscreen.insert(kCloseButton);
  visible_in_fullscreen.insert(kExclusiveScreenToast);
  visible_in_fullscreen.insert(kController);
  visible_in_fullscreen.insert(kControllerTouchpadButton);
  visible_in_fullscreen.insert(kControllerAppButton);
  visible_in_fullscreen.insert(kControllerHomeButton);
  visible_in_fullscreen.insert(kLaser);
  visible_in_fullscreen.insert(kReticle);
  visible_in_fullscreen.insert(kContentFrame);
  visible_in_fullscreen.insert(kContentFrameHitPlane);

  CreateScene(kNotInCct, kNotInWebVr);

  // Hold onto the background color to make sure it changes.
  SkColor initial_background = SK_ColorBLACK;
  GetBackgroundColor(&initial_background);
  VerifyOnlyElementsVisible("Initial", kElementsVisibleInBrowsing);
  UiElement* content_quad = scene_->GetUiElementByName(kContentQuad);
  UiElement* content_group =
      scene_->GetUiElementByName(k2dBrowsingContentGroup);
  gfx::SizeF initial_content_size = content_quad->size();
  gfx::Transform initial_position = content_group->LocalTransform();

  // In fullscreen mode, content elements should be visible, control elements
  // should be hidden.
  ui_->SetFullscreen(true);
  VerifyOnlyElementsVisible("In fullscreen", visible_in_fullscreen);
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
  EXPECT_TRUE(RunFor(MsToDelta(1000)));
  EXPECT_FALSE(IsAnimating(content_quad, {BOUNDS}));
  EXPECT_FALSE(IsAnimating(content_group, {TRANSFORM}));
  EXPECT_NE(initial_content_size, content_quad->size());
  EXPECT_NE(initial_position, content_group->LocalTransform());

  // Everything should return to original state after leaving fullscreen.
  ui_->SetFullscreen(false);
  VerifyOnlyElementsVisible("Restore initial", kElementsVisibleInBrowsing);
  SkColor no_longer_fullscreen_background = SK_ColorBLACK;
  GetBackgroundColor(&no_longer_fullscreen_background);
  EXPECT_EQ(
      ColorScheme::GetColorScheme(ColorScheme::kModeNormal).world_background,
      no_longer_fullscreen_background);
  // Should have started transition.
  EXPECT_TRUE(IsAnimating(content_quad, {BOUNDS}));
  EXPECT_TRUE(IsAnimating(content_group, {TRANSFORM}));
  // Finish the transition.
  EXPECT_TRUE(RunFor(MsToDelta(1000)));
  EXPECT_FALSE(IsAnimating(content_quad, {BOUNDS}));
  EXPECT_FALSE(IsAnimating(content_group, {TRANSFORM}));
  EXPECT_EQ(initial_content_size, content_quad->size());
  EXPECT_EQ(initial_position, content_group->LocalTransform());
}

TEST_F(UiTest, SecurityIconClickTriggersUnsupportedMode) {
  CreateScene(kNotInCct, kNotInWebVr);

  // Initial state.
  VerifyOnlyElementsVisible("Initial", kElementsVisibleInBrowsing);

  // Clicking on security icon should trigger unsupported mode.
  EXPECT_CALL(*browser_,
              OnUnsupportedMode(UiUnsupportedMode::kUnhandledPageInfo));
  browser_->OnUnsupportedMode(UiUnsupportedMode::kUnhandledPageInfo);
  VerifyOnlyElementsVisible("Prompt invisible", kElementsVisibleInBrowsing);
}

TEST_F(UiTest, ClickingOmniboxTriggersUnsupportedMode) {
  UiInitialState state;
  state.needs_keyboard_update = true;
  CreateScene(state);
  VerifyOnlyElementsVisible("Initial", kElementsVisibleInBrowsing);

  // Clicking the omnibox should show the update prompt.
  auto* omnibox = scene_->GetUiElementByName(kUrlBarOriginContent);
  EXPECT_CALL(*browser_,
              OnUnsupportedMode(UiUnsupportedMode::kNeedsKeyboardUpdate));
  omnibox->OnButtonUp({0, 10});
  ui_->ShowExitVrPrompt(UiUnsupportedMode::kNeedsKeyboardUpdate);
  OnBeginFrame();
  EXPECT_EQ(model_->active_modal_prompt_type,
            ModalPromptType::kModalPromptTypeUpdateKeyboard);
  EXPECT_TRUE(scene_->GetUiElementByName(kUpdateKeyboardPrompt)->IsVisible());
}

TEST_F(UiTest, WebInputEditingTriggersUnsupportedMode) {
  UiInitialState state;
  state.needs_keyboard_update = true;
  CreateScene(state);
  VerifyOnlyElementsVisible("Initial", kElementsVisibleInBrowsing);

  // A call to show the keyboard should show the update prompt.
  EXPECT_CALL(*browser_,
              OnUnsupportedMode(UiUnsupportedMode::kNeedsKeyboardUpdate));
  ui_->ShowSoftInput(true);
  ui_->ShowExitVrPrompt(UiUnsupportedMode::kNeedsKeyboardUpdate);
  OnBeginFrame();
  EXPECT_EQ(model_->active_modal_prompt_type,
            ModalPromptType::kModalPromptTypeUpdateKeyboard);
  EXPECT_TRUE(scene_->GetUiElementByName(kUpdateKeyboardPrompt)->IsVisible());
}

TEST_F(UiTest, ExitWebInputEditingOnAppButtonClick) {
  CreateScene(kNotInCct, kNotInWebVr);
  EXPECT_FALSE(scene_->GetUiElementByName(kKeyboard)->IsVisible());
  ui_->ShowSoftInput(true);
  OnBeginFrame();
  EXPECT_TRUE(scene_->GetUiElementByName(kKeyboard)->IsVisible());
  ui_->OnAppButtonClicked();
  OnBeginFrame();
  // Clicking app button should hide the keyboard.
  EXPECT_FALSE(scene_->GetUiElementByName(kKeyboard)->IsVisible());
}

TEST_F(UiTest, UiUpdatesForShowingExitPrompt) {
  CreateScene(kNotInCct, kNotInWebVr);

  // Initial state.
  VerifyOnlyElementsVisible("Initial", kElementsVisibleInBrowsing);

  // Showing exit VR prompt should make prompt visible.
  model_->active_modal_prompt_type = kModalPromptTypeExitVRForSiteInfo;
  VerifyOnlyElementsVisible("Prompt visible", kElementsVisibleWithExitPrompt);
}

TEST_F(UiTest, UiUpdatesForHidingExitPrompt) {
  CreateScene(kNotInCct, kNotInWebVr);

  // Initial state.
  model_->active_modal_prompt_type = kModalPromptTypeExitVRForSiteInfo;
  VerifyOnlyElementsVisible("Initial", kElementsVisibleWithExitPrompt);

  // Hiding exit VR prompt should make prompt invisible.
  model_->active_modal_prompt_type = kModalPromptTypeNone;
  EXPECT_TRUE(RunFor(MsToDelta(1000)));
  VerifyOnlyElementsVisible("Prompt invisible", kElementsVisibleInBrowsing);
}

TEST_F(UiTest, BackplaneClickTriggersOnExitPrompt) {
  CreateScene(kNotInCct, kNotInWebVr);

  // Initial state.
  VerifyOnlyElementsVisible("Initial", kElementsVisibleInBrowsing);
  ui_->ShowExitVrPrompt(UiUnsupportedMode::kUnhandledPageInfo);

  VerifyOnlyElementsVisible("Prompt visible", kElementsVisibleWithExitPrompt);

  // Click on backplane should trigger UI browser interface but not close
  // prompt.
  EXPECT_CALL(*browser_,
              OnExitVrPromptResult(ExitVrPromptChoice::CHOICE_NONE,
                                   UiUnsupportedMode::kUnhandledPageInfo));
  scene_->GetUiElementByName(kExitPromptBackplane)->OnButtonUp(gfx::PointF());

  VerifyOnlyElementsVisible("Prompt cleared", kElementsVisibleInBrowsing);
}

TEST_F(UiTest, PrimaryButtonClickTriggersOnExitPrompt) {
  CreateScene(kNotInCct, kNotInWebVr);

  // Initial state.
  VerifyOnlyElementsVisible("Initial", kElementsVisibleInBrowsing);
  model_->active_modal_prompt_type = kModalPromptTypeExitVRForSiteInfo;
  OnBeginFrame();

  // Click on 'OK' should trigger UI browser interface and close prompt.
  EXPECT_CALL(*browser_,
              OnExitVrPromptResult(ExitVrPromptChoice::CHOICE_STAY,
                                   UiUnsupportedMode::kUnhandledPageInfo));
  static_cast<ExitPrompt*>(scene_->GetUiElementByName(kExitPrompt))
      ->ClickPrimaryButtonForTesting();
  VerifyOnlyElementsVisible("Prompt cleared", kElementsVisibleInBrowsing);
}

TEST_F(UiTest, SecondaryButtonClickTriggersOnExitPrompt) {
  CreateScene(kNotInCct, kNotInWebVr);

  // Initial state.
  VerifyOnlyElementsVisible("Initial", kElementsVisibleInBrowsing);
  model_->active_modal_prompt_type = kModalPromptTypeExitVRForSiteInfo;
  OnBeginFrame();

  // Click on 'Exit VR' should trigger UI browser interface and close prompt.
  EXPECT_CALL(*browser_,
              OnExitVrPromptResult(ExitVrPromptChoice::CHOICE_EXIT,
                                   UiUnsupportedMode::kUnhandledPageInfo));

  static_cast<ExitPrompt*>(scene_->GetUiElementByName(kExitPrompt))
      ->ClickSecondaryButtonForTesting();
  VerifyOnlyElementsVisible("Prompt cleared", kElementsVisibleInBrowsing);
}

TEST_F(UiTest, UiUpdatesForWebVR) {
  CreateScene(kNotInCct, kInWebVr);

  model_->capturing_state.audio_capture_enabled = true;
  model_->capturing_state.video_capture_enabled = true;
  model_->capturing_state.screen_capture_enabled = true;
  model_->capturing_state.location_access_enabled = true;
  model_->capturing_state.bluetooth_connected = true;

  VerifyOnlyElementsVisible("Elements hidden",
                            std::set<UiElementName>{kWebVrBackground});
}

// This test verifies that we ignore the WebVR frame when we're not expecting
// WebVR presentation. You can get an unexpected frame when for example, the
// user hits the app button to exit WebVR mode, but the site continues to pump
// frames. If the frame is not ignored, our UI will think we're in WebVR mode.
TEST_F(UiTest, WebVrFramesIgnoredWhenUnexpected) {
  CreateScene(kNotInCct, kInWebVr);

  ui_->OnWebVrFrameAvailable();
  VerifyOnlyElementsVisible("Elements hidden", std::set<UiElementName>{});
  // Disable WebVR mode.
  ui_->SetWebVrMode(false, false);

  // New frame available after exiting WebVR mode.
  ui_->OnWebVrFrameAvailable();
  VerifyOnlyElementsVisible("Browser visible", kElementsVisibleInBrowsing);
}

TEST_F(UiTest, UiUpdateTransitionToWebVR) {
  CreateScene(kNotInCct, kNotInWebVr);
  model_->capturing_state.audio_capture_enabled = true;
  model_->capturing_state.video_capture_enabled = true;
  model_->capturing_state.screen_capture_enabled = true;
  model_->capturing_state.location_access_enabled = true;
  model_->capturing_state.bluetooth_connected = true;

  // Transition to WebVR mode
  ui_->SetWebVrMode(true, false);
  ui_->OnWebVrFrameAvailable();

  // All elements should be hidden.
  VerifyOnlyElementsVisible("Elements hidden", std::set<UiElementName>{});
}

TEST_F(UiTest, CaptureIndicatorsVisibility) {
  const std::set<UiElementName> indicators = {
      kAudioCaptureIndicator,       kVideoCaptureIndicator,
      kScreenCaptureIndicator,      kLocationAccessIndicator,
      kBluetoothConnectedIndicator,
  };

  CreateScene(kNotInCct, kNotInWebVr);
  EXPECT_TRUE(VerifyVisibility(indicators, false));
  EXPECT_TRUE(VerifyRequiresLayout(indicators, false));

  model_->capturing_state.audio_capture_enabled = true;
  model_->capturing_state.video_capture_enabled = true;
  model_->capturing_state.screen_capture_enabled = true;
  model_->capturing_state.location_access_enabled = true;
  model_->capturing_state.bluetooth_connected = true;
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
  EXPECT_TRUE(VerifyVisibility(indicators, true));
  EXPECT_TRUE(VerifyRequiresLayout(indicators, true));

  // Ensure they can be turned off.
  model_->capturing_state.audio_capture_enabled = false;
  model_->capturing_state.video_capture_enabled = false;
  model_->capturing_state.screen_capture_enabled = false;
  model_->capturing_state.location_access_enabled = false;
  model_->capturing_state.bluetooth_connected = false;
  EXPECT_TRUE(VerifyRequiresLayout(indicators, false));
}

TEST_F(UiTest, PropagateContentBoundsOnStart) {
  CreateScene(kNotInCct, kNotInWebVr);

  gfx::SizeF expected_bounds(0.495922f, 0.330614f);
  EXPECT_CALL(*browser_,
              OnContentScreenBoundsChanged(
                  SizeFsAreApproximatelyEqual(expected_bounds, kTolerance)));

  ui_->OnProjMatrixChanged(kPixelDaydreamProjMatrix);
  OnBeginFrame();
}

TEST_F(UiTest, PropagateContentBoundsOnFullscreen) {
  CreateScene(kNotInCct, kNotInWebVr);

  ui_->OnProjMatrixChanged(kPixelDaydreamProjMatrix);
  ui_->SetFullscreen(true);

  gfx::SizeF expected_bounds(0.587874f, 0.330614f);
  EXPECT_CALL(*browser_,
              OnContentScreenBoundsChanged(
                  SizeFsAreApproximatelyEqual(expected_bounds, kTolerance)));

  ui_->OnProjMatrixChanged(kPixelDaydreamProjMatrix);
  OnBeginFrame();
}

TEST_F(UiTest, DontPropagateContentBoundsOnNegligibleChange) {
  CreateScene(kNotInCct, kNotInWebVr);

  EXPECT_TRUE(RunFor(MsToDelta(0)));
  ui_->OnProjMatrixChanged(kPixelDaydreamProjMatrix);

  UiElement* content_quad = scene_->GetUiElementByName(kContentQuad);
  gfx::SizeF content_quad_size = content_quad->size();
  content_quad_size.Scale(1.2f);
  content_quad->SetSize(content_quad_size.width(), content_quad_size.height());
  OnBeginFrame();

  EXPECT_CALL(*browser_, OnContentScreenBoundsChanged(testing::_)).Times(0);

  ui_->OnProjMatrixChanged(kPixelDaydreamProjMatrix);
}

TEST_F(UiTest, RendererUsesCorrectOpacity) {
  CreateScene(kNotInCct, kNotInWebVr);

  ContentElement* content_element =
      static_cast<ContentElement*>(scene_->GetUiElementByName(kContentQuad));
  content_element->SetTextureId(0);
  content_element->SetTextureLocation(UiElementRenderer::kTextureLocationLocal);
  TexturedElement::SetInitializedForTesting();

  CheckRendererOpacityRecursive(&scene_->root_element());
}

TEST_F(UiTest, LoadingIndicatorBindings) {
  CreateScene(kNotInCct, kNotInWebVr);
  model_->loading = true;
  model_->load_progress = 0.5f;
  EXPECT_TRUE(VerifyVisibility({kLoadingIndicator}, true));
  UiElement* loading_indicator = scene_->GetUiElementByName(kLoadingIndicator);
  UiElement* loading_indicator_fg = loading_indicator->children().back().get();
  EXPECT_FLOAT_EQ(loading_indicator->size().width() * 0.5f,
                  loading_indicator_fg->size().width());
  float tx =
      loading_indicator_fg->GetTargetTransform().Apply().matrix().get(0, 3);
  EXPECT_FLOAT_EQ(loading_indicator->size().width() * 0.25f, tx);
}

TEST_F(UiTest, ExitWarning) {
  CreateScene(kNotInCct, kNotInWebVr);
  ui_->SetIsExiting();
  EXPECT_TRUE(VerifyVisibility(kElementsVisibleWithExitWarning, true));
}

TEST_F(UiTest, WebVrTimeout) {
  CreateScene(kNotInCct, kInWebVr);

  ui_->SetWebVrMode(true, false);
  model_->web_vr.state = kWebVrAwaitingFirstFrame;

  RunFor(MsToDelta(500));
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
          kWebVrBackground,
      },
      true);

  model_->web_vr.state = kWebVrTimeoutImminent;
  RunFor(MsToDelta(500));
  VerifyVisibility(
      {
          kWebVrTimeoutMessage, kWebVrTimeoutMessageLayout,
          kWebVrTimeoutMessageIcon, kWebVrTimeoutMessageText,
          kWebVrTimeoutMessageButton, kWebVrTimeoutMessageButtonText,
      },
      false);
  VerifyVisibility(
      {
          kWebVrTimeoutSpinner, kWebVrBackground,
      },
      true);

  model_->web_vr.state = kWebVrTimedOut;
  RunFor(MsToDelta(500));
  VerifyVisibility(
      {
          kWebVrTimeoutSpinner,
      },
      false);
  VerifyVisibility(
      {
          kWebVrBackground, kWebVrTimeoutMessage, kWebVrTimeoutMessageLayout,
          kWebVrTimeoutMessageIcon, kWebVrTimeoutMessageText,
          kWebVrTimeoutMessageButton, kWebVrTimeoutMessageButtonText,
      },
      true);
}

TEST_F(UiTest, SpeechRecognitionUiVisibility) {
  CreateScene(kNotInCct, kNotInWebVr);

  ui_->SetSpeechRecognitionEnabled(true);

  // Start hiding browsing foreground and showing speech recognition listening
  // UI.
  EXPECT_TRUE(RunFor(MsToDelta(10)));
  VerifyIsAnimating({k2dBrowsingForeground}, {OPACITY}, true);
  VerifyIsAnimating({kSpeechRecognitionListening, kSpeechRecognitionResult},
                    {OPACITY}, false);
  ui_->SetSpeechRecognitionEnabled(true);
  EXPECT_TRUE(RunFor(MsToDelta(10)));
  VerifyIsAnimating({k2dBrowsingForeground, kSpeechRecognitionListening},
                    {OPACITY}, true);

  EXPECT_TRUE(RunFor(MsToDelta(kSpeechRecognitionOpacityAnimationDurationMs)));
  // All opacity animations should be finished at this point.
  VerifyIsAnimating({k2dBrowsingForeground, kSpeechRecognitionListening,
                     kSpeechRecognitionResult},
                    {OPACITY}, false);
  VerifyIsAnimating({kSpeechRecognitionListeningGrowingCircle}, {CIRCLE_GROW},
                    false);
  VerifyVisibility(kElementsVisibleWithVoiceSearch, true);
  VerifyVisibility({k2dBrowsingForeground, kSpeechRecognitionResult}, false);

  model_->speech.speech_recognition_state = SPEECH_RECOGNITION_READY;
  EXPECT_TRUE(RunFor(MsToDelta(10)));
  VerifyIsAnimating({kSpeechRecognitionListeningGrowingCircle}, {CIRCLE_GROW},
                    true);

  // Mock received speech result.
  model_->speech.recognition_result = base::ASCIIToUTF16("test");
  model_->speech.speech_recognition_state = SPEECH_RECOGNITION_END;
  ui_->SetSpeechRecognitionEnabled(false);

  EXPECT_TRUE(RunFor(MsToDelta(10)));
  VerifyVisibility(kElementsVisibleWithVoiceSearchResult, true);
  // Speech result UI should show instantly while listening UI hide immediately.
  VerifyIsAnimating({k2dBrowsingForeground, kSpeechRecognitionListening,
                     kSpeechRecognitionResult},
                    {OPACITY}, false);
  VerifyIsAnimating({kSpeechRecognitionListeningGrowingCircle}, {CIRCLE_GROW},
                    false);
  VerifyVisibility({k2dBrowsingForeground, kSpeechRecognitionListening}, false);

  // The visibility of Speech Recognition UI should not change at this point.
  EXPECT_FALSE(RunFor(MsToDelta(10)));

  EXPECT_TRUE(RunFor(MsToDelta(kSpeechRecognitionResultTimeoutSeconds * 1000)));
  // Start hide speech recognition result and show browsing foreground.
  VerifyIsAnimating({k2dBrowsingForeground, kSpeechRecognitionResult},
                    {OPACITY}, true);
  VerifyIsAnimating({kSpeechRecognitionListening}, {OPACITY}, false);

  EXPECT_TRUE(RunFor(MsToDelta(kSpeechRecognitionOpacityAnimationDurationMs)));
  VerifyIsAnimating({k2dBrowsingForeground, kSpeechRecognitionListening,
                     kSpeechRecognitionResult},
                    {OPACITY}, false);

  // Visibility is as expected.
  VerifyVisibility({kSpeechRecognitionListening, kSpeechRecognitionResult},
                   false);

  EXPECT_TRUE(IsVisible(k2dBrowsingForeground));
}

TEST_F(UiTest, SpeechRecognitionUiVisibilityNoResult) {
  CreateScene(kNotInCct, kNotInWebVr);

  ui_->SetSpeechRecognitionEnabled(true);
  EXPECT_TRUE(RunFor(MsToDelta(kSpeechRecognitionOpacityAnimationDurationMs)));
  VerifyIsAnimating({k2dBrowsingForeground, kSpeechRecognitionListening},
                    {OPACITY}, false);

  // Mock exit without a recognition result
  ui_->SetSpeechRecognitionEnabled(false);
  model_->speech.recognition_result.clear();
  model_->speech.speech_recognition_state = SPEECH_RECOGNITION_END;

  EXPECT_TRUE(RunFor(MsToDelta(10)));
  VerifyIsAnimating({k2dBrowsingForeground, kSpeechRecognitionListening},
                    {OPACITY}, true);
  VerifyIsAnimating({kSpeechRecognitionResult}, {OPACITY}, false);

  EXPECT_TRUE(RunFor(MsToDelta(kSpeechRecognitionOpacityAnimationDurationMs)));
  VerifyIsAnimating({k2dBrowsingForeground, kSpeechRecognitionListening,
                     kSpeechRecognitionResult},
                    {OPACITY}, false);

  // Visibility is as expected.
  VerifyVisibility({kSpeechRecognitionListening, kSpeechRecognitionResult},
                   false);
  EXPECT_TRUE(IsVisible(k2dBrowsingForeground));
}

TEST_F(UiTest, OmniboxSuggestionBindings) {
  CreateScene(kNotInCct, kNotInWebVr);
  UiElement* container = scene_->GetUiElementByName(kOmniboxSuggestions);
  ASSERT_NE(container, nullptr);

  OnBeginFrame();
  EXPECT_EQ(container->children().size(), 0u);

  model_->push_mode(kModeEditingOmnibox);
  OnBeginFrame();
  EXPECT_EQ(container->children().size(), 0u);
  EXPECT_EQ(NumVisibleInTree(kOmniboxSuggestions), 1);

  model_->omnibox_suggestions.emplace_back(OmniboxSuggestion(
      base::string16(), base::string16(), ACMatchClassifications(),
      ACMatchClassifications(), AutocompleteMatch::Type::VOICE_SUGGEST, GURL(),
      base::string16(), base::string16()));
  OnBeginFrame();
  EXPECT_EQ(container->children().size(), 1u);
  EXPECT_GT(NumVisibleInTree(kOmniboxSuggestions), 1);

  model_->omnibox_suggestions.clear();
  OnBeginFrame();
  EXPECT_EQ(container->children().size(), 0u);
  EXPECT_EQ(NumVisibleInTree(kOmniboxSuggestions), 1);
}

TEST_F(UiTest, OmniboxSuggestionNavigates) {
  CreateScene(kNotInCct, kNotInWebVr);
  GURL gurl("http://test.com/");
  model_->omnibox_suggestions.emplace_back(OmniboxSuggestion(
      base::string16(), base::string16(), ACMatchClassifications(),
      ACMatchClassifications(), AutocompleteMatch::Type::VOICE_SUGGEST, gurl,
      base::string16(), base::string16()));
  OnBeginFrame();

  UiElement* suggestions = scene_->GetUiElementByName(kOmniboxSuggestions);
  ASSERT_NE(suggestions, nullptr);
  UiElement* suggestion = suggestions->children().front().get();
  ASSERT_NE(suggestion, nullptr);
  EXPECT_CALL(*browser_,
              Navigate(gurl, NavigationMethod::kOmniboxSuggestionSelected))
      .Times(1);
  suggestion->OnHoverEnter({0, 0});
  suggestion->OnButtonDown({0, 0});
  suggestion->OnButtonUp({0, 0});
}

TEST_F(UiTest, ControllerQuiescence) {
  CreateScene(kNotInCct, kNotInWebVr);
  OnBeginFrame();
  EXPECT_TRUE(IsVisible(kControllerGroup));
  model_->skips_redraw_when_not_dirty = true;
  model_->controller.quiescent = true;

  UiElement* controller_group = scene_->GetUiElementByName(kControllerGroup);
  EXPECT_TRUE(RunFor(MsToDelta(100)));
  EXPECT_LT(0.0f, controller_group->computed_opacity());
  EXPECT_TRUE(RunFor(MsToDelta(500)));
  EXPECT_EQ(0.0f, controller_group->computed_opacity());

  model_->controller.quiescent = false;
  EXPECT_TRUE(RunFor(MsToDelta(100)));
  EXPECT_GT(1.0f, controller_group->computed_opacity());
  EXPECT_TRUE(RunFor(MsToDelta(150)));
  EXPECT_EQ(1.0f, controller_group->computed_opacity());

  model_->skips_redraw_when_not_dirty = false;
  model_->controller.quiescent = true;
  EXPECT_FALSE(RunFor(MsToDelta(1000)));
  EXPECT_TRUE(IsVisible(kControllerGroup));
}

TEST_F(UiTest, CloseButtonColorBindings) {
  CreateScene(kInCct, kNotInWebVr);
  EXPECT_TRUE(IsVisible(kCloseButton));
  DiscButton* button =
      static_cast<DiscButton*>(scene_->GetUiElementByName(kCloseButton));
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

TEST_F(UiTest, ExitPresentAndFullscreenOnAppButtonClick) {
  CreateScene(kNotInCct, kNotInWebVr);
  ui_->SetWebVrMode(true, false);
  // Clicking app button should trigger to exit presentation.
  EXPECT_CALL(*browser_, ExitPresent());
  // And also trigger exit fullscreen.
  EXPECT_CALL(*browser_, ExitFullscreen());
  ui_->OnAppButtonClicked();
}

// Tests that transient elements will show, even if there is a long delay
// between when they are asked to appear and when we issue the first frame with
// them visible.
TEST_F(UiTest, TransientToastsWithDelayedFirstFrame) {
  CreateSceneForAutoPresentation();

  // Initially, we should only show the splash screen.
  VerifyOnlyElementsVisible("Initial", {kSplashScreenText, kWebVrBackground});
  // Enter WebVR with autopresentation.
  ui_->SetWebVrMode(true, false);
  EXPECT_TRUE(RunFor(MsToDelta(1000 * kSplashScreenMinDurationSeconds)));
  VerifyOnlyElementsVisible("Initial", {kSplashScreenText, kWebVrBackground});

  EXPECT_FALSE(RunFor(MsToDelta(2000)));
  ui_->OnWebVrTimeoutImminent();
  EXPECT_TRUE(RunFor(MsToDelta(3000)));
  ui_->OnWebVrTimedOut();
  EXPECT_TRUE(RunFor(MsToDelta(5000)));
  ui_->OnWebVrFrameAvailable();
  OnBeginFrame();

  // If we advance far beyond the timeout for our first frame and our logic was
  // naive, we would start to hide the transient element.
  OnBeginFrame(MsToDelta(40000));
  VerifyOnlyElementsVisible("Autopresented", {kWebVrUrlToast});
}

TEST_F(UiTest, DefaultBackgroundWhenNoAssetAvailable) {
  UiInitialState state;
  state.assets_supported = false;
  CreateScene(state);

  EXPECT_FALSE(IsVisible(k2dBrowsingTexturedBackground));
  EXPECT_TRUE(IsVisible(k2dBrowsingDefaultBackground));
  EXPECT_TRUE(IsVisible(kContentQuad));
}

TEST_F(UiTest, TextureBackgroundAfterAssetLoaded) {
  UiInitialState state;
  state.assets_supported = true;
  CreateScene(state);

  EXPECT_FALSE(IsVisible(k2dBrowsingTexturedBackground));
  EXPECT_FALSE(IsVisible(k2dBrowsingDefaultBackground));
  EXPECT_FALSE(IsVisible(kContentQuad));

  auto assets = std::make_unique<Assets>();
  ui_->OnAssetsLoaded(AssetsLoadStatus::kSuccess, std::move(assets),
                      base::Version("1.0"));

  EXPECT_TRUE(IsVisible(k2dBrowsingTexturedBackground));
  EXPECT_TRUE(IsVisible(kContentQuad));
  EXPECT_FALSE(IsVisible(k2dBrowsingDefaultBackground));
}

TEST_F(UiTest, ControllerLabels) {
  CreateScene(kNotInCct, kNotInWebVr);

  EXPECT_FALSE(IsVisible(kControllerTrackpadLabel));
  EXPECT_FALSE(IsVisible(kControllerTrackpadRepositionLabel));
  EXPECT_FALSE(IsVisible(kControllerExitButtonLabel));
  EXPECT_FALSE(IsVisible(kControllerBackButtonLabel));

  model_->controller.resting_in_viewport = true;
  EXPECT_TRUE(IsVisible(kControllerTrackpadLabel));
  EXPECT_FALSE(IsVisible(kControllerTrackpadRepositionLabel));
  EXPECT_FALSE(IsVisible(kControllerExitButtonLabel));
  EXPECT_FALSE(IsVisible(kControllerBackButtonLabel));

  model_->push_mode(kModeFullscreen);
  EXPECT_TRUE(IsVisible(kControllerTrackpadLabel));
  EXPECT_FALSE(IsVisible(kControllerTrackpadRepositionLabel));
  EXPECT_TRUE(IsVisible(kControllerExitButtonLabel));
  EXPECT_FALSE(IsVisible(kControllerBackButtonLabel));

  model_->pop_mode(kModeFullscreen);
  EXPECT_TRUE(IsVisible(kControllerTrackpadLabel));
  EXPECT_FALSE(IsVisible(kControllerTrackpadRepositionLabel));
  EXPECT_FALSE(IsVisible(kControllerExitButtonLabel));
  EXPECT_FALSE(IsVisible(kControllerBackButtonLabel));

  model_->push_mode(kModeEditingOmnibox);
  EXPECT_TRUE(IsVisible(kControllerTrackpadLabel));
  EXPECT_FALSE(IsVisible(kControllerTrackpadRepositionLabel));
  EXPECT_FALSE(IsVisible(kControllerExitButtonLabel));
  EXPECT_TRUE(IsVisible(kControllerBackButtonLabel));

  model_->pop_mode(kModeEditingOmnibox);
  EXPECT_TRUE(IsVisible(kControllerTrackpadLabel));
  EXPECT_FALSE(IsVisible(kControllerTrackpadRepositionLabel));
  EXPECT_FALSE(IsVisible(kControllerExitButtonLabel));
  EXPECT_FALSE(IsVisible(kControllerBackButtonLabel));

  model_->push_mode(kModeVoiceSearch);
  EXPECT_TRUE(IsVisible(kControllerTrackpadLabel));
  EXPECT_FALSE(IsVisible(kControllerTrackpadRepositionLabel));
  EXPECT_FALSE(IsVisible(kControllerExitButtonLabel));
  EXPECT_TRUE(IsVisible(kControllerBackButtonLabel));

  model_->pop_mode(kModeVoiceSearch);
  EXPECT_TRUE(IsVisible(kControllerTrackpadLabel));
  EXPECT_FALSE(IsVisible(kControllerTrackpadRepositionLabel));
  EXPECT_FALSE(IsVisible(kControllerExitButtonLabel));
  EXPECT_FALSE(IsVisible(kControllerBackButtonLabel));

  model_->push_mode(kModeRepositionWindow);
  model_->controller.laser_direction = kForwardVector;
  EXPECT_FALSE(IsVisible(kControllerTrackpadLabel));
  EXPECT_TRUE(IsVisible(kControllerTrackpadRepositionLabel));
  EXPECT_FALSE(IsVisible(kControllerExitButtonLabel));
  EXPECT_TRUE(IsVisible(kControllerBackButtonLabel));

  model_->controller.resting_in_viewport = false;
  EXPECT_FALSE(IsVisible(kControllerTrackpadRepositionLabel));
  EXPECT_FALSE(IsVisible(kControllerTrackpadLabel));
  EXPECT_FALSE(IsVisible(kControllerExitButtonLabel));
  EXPECT_FALSE(IsVisible(kControllerBackButtonLabel));
}

TEST_F(UiTest, ResetRepositioner) {
  CreateScene(kNotInCct, kNotInWebVr);

  Repositioner* repositioner = static_cast<Repositioner*>(
      scene_->GetUiElementByName(k2dBrowsingRepositioner));

  OnBeginFrame();
  gfx::Transform original = repositioner->world_space_transform();

  repositioner->set_laser_direction(kForwardVector);
  repositioner->SetEnabled(true);
  repositioner->set_laser_direction({0, 1, 0});
  OnBeginFrame();

  EXPECT_NE(original, repositioner->world_space_transform());
  repositioner->SetEnabled(false);
  model_->controller.recentered = true;

  OnBeginFrame();
  EXPECT_EQ(original, repositioner->world_space_transform());
}

// No element in the controller root's subtree should be hit testable.
TEST_F(UiTest, ControllerHitTest) {
  CreateScene(kNotInCct, kNotInWebVr);
  auto* controller = scene_->GetUiElementByName(kControllerRoot);
  for (auto& child : *controller)
    EXPECT_FALSE(child.IsHitTestable());
}

TEST_F(UiTest, BrowsingRootBounds) {
  CreateScene(kNotInCct, kNotInWebVr);
  auto* elem = scene_->GetUiElementByName(k2dBrowsingContentGroup);
  auto* root = scene_->GetUiElementByName(k2dBrowsingRepositioner);
  for (; elem; elem = elem->parent()) {
    int num_bounds_contributors = 0;
    for (auto& child : elem->children()) {
      if (child->contributes_to_parent_bounds())
        num_bounds_contributors++;
    }
    EXPECT_EQ(1, num_bounds_contributors);
    if (elem == root)
      break;
  }
}

}  // namespace vr
