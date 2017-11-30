// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_scene_creator.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/ranges.h"
#include "base/stl_util.h"
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
#include "chrome/browser/vr/test/mock_ui_browser_interface.h"
#include "chrome/browser/vr/test/ui_test.h"
#include "chrome/browser/vr/ui_renderer.h"
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
    kBackgroundFront,   kBackgroundLeft, kBackgroundBack,
    kBackgroundRight,   kBackgroundTop,  kBackgroundBottom,
    kCeiling,           kFloor,          kContentQuad,
    kBackplane,         kUrlBar,         kUnderDevelopmentNotice,
    kController,        kReticle,        kLaser,
    kVoiceSearchButton,
};
const std::set<UiElementName> kElementsVisibleWithExitPrompt = {
    kBackgroundFront, kBackgroundLeft,      kBackgroundBack, kBackgroundRight,
    kBackgroundTop,   kBackgroundBottom,    kCeiling,        kFloor,
    kExitPrompt,      kExitPromptBackplane, kController,     kReticle,
    kLaser,
};
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
    kOmniboxContainer,
    kOmniboxTextField,
    kOmniboxSuggestions,
    kLoadingIndicator,
    kWebVrTimeoutSpinner,
    kWebVrTimeoutMessage,
    kWebVrTimeoutMessageIcon,
    kWebVrTimeoutMessageText,
    kWebVrTimeoutMessageButtonText,
    kSpeechRecognitionResultBackplane,
};
const std::set<UiElementName> kSpecialHitTestableElements = {
    kCloseButton,        kWebVrTimeoutMessageButton,
    kVoiceSearchButton,  kSpeechRecognitionListeningCloseButton,
    kOmniboxCloseButton,
};
const std::set<UiElementName> kElementsVisibleWithExitWarning = {
    kScreenDimmer, kExitWarning,
};
const std::vector<std::string> kElementsInDrawOrder = {
    "kBackgroundFront",
    "kBackgroundLeft",
    "kBackgroundBack",
    "kBackgroundRight",
    "kBackgroundTop",
    "kBackgroundBottom",
    "kScreenDimmer",
    "kSplashScreenBackground",
    "kWebVrTimeoutSpinnerBackground",
    "kFloor",
    "kCeiling",
    "kBackplane",
    "kContentQuad",
    "kAudioCaptureIndicator",
    "kVideoCaptureIndicator",
    "kScreenCaptureIndicator",
    "kBluetoothConnectedIndicator",
    "kLocationAccessIndicator",
    "kUrlBar",
    "kLoadingIndicator",
    "kLoadingIndicatorForeground",
    "kUnderDevelopmentNotice",
    "kVoiceSearchButton",
    "kVoiceSearchButton:kTypeButtonBackground",
    "kVoiceSearchButton:kTypeButtonForeground",
    "kVoiceSearchButton:kTypeButtonHitTarget",
    "kCloseButton",
    "kCloseButton:kTypeButtonBackground",
    "kCloseButton:kTypeButtonForeground",
    "kCloseButton:kTypeButtonHitTarget",
    "kExclusiveScreenToast",
    "kExitPromptBackplane",
    "kExitPrompt",
    "kAudioPermissionPromptBackplane",
    "kAudioPermissionPromptShadow",
    "kAudioPermissionPrompt",
    "kOmniboxContainer",
    "kOmniboxTextField",
    "kOmniboxCloseButton",
    "kOmniboxCloseButton:kTypeButtonBackground",
    "kOmniboxCloseButton:kTypeButtonForeground",
    "kOmniboxCloseButton:kTypeButtonHitTarget",
    "kSpeechRecognitionResultText",
    "kSpeechRecognitionResultCircle",
    "kSpeechRecognitionResultMicrophoneIcon",
    "kSpeechRecognitionResultBackplane",
    "kSpeechRecognitionListeningGrowingCircle",
    "kSpeechRecognitionListeningInnerCircle",
    "kSpeechRecognitionListeningMicrophoneIcon",
    "kSpeechRecognitionListeningCloseButton",
    "kSpeechRecognitionListeningCloseButton:kTypeButtonBackground",
    "kSpeechRecognitionListeningCloseButton:kTypeButtonForeground",
    "kSpeechRecognitionListeningCloseButton:kTypeButtonHitTarget",
    "kController",
    "kLaser",
    "kReticle",
    "kExitWarning",
    "kWebVrUrlToast",
    "kExclusiveScreenToastViewportAware",
    "kSplashScreenText",
    "kWebVrTimeoutSpinner",
    "kWebVrTimeoutMessage",
    "kWebVrTimeoutMessageIcon",
    "kWebVrTimeoutMessageText",
    "kWebVrTimeoutMessageButton",
    "kWebVrTimeoutMessageButton:kTypeButtonBackground",
    "kWebVrTimeoutMessageButton:kTypeButtonForeground",
    "kWebVrTimeoutMessageButton:kTypeButtonHitTarget",
    "kWebVrTimeoutMessageButtonText",
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

  EXPECT_TRUE(OnBeginFrame());
  EXPECT_TRUE(IsVisible(kVoiceSearchButton));

  model_->incognito = true;
  EXPECT_TRUE(OnBeginFrame());
  EXPECT_FALSE(IsVisible(kVoiceSearchButton));
}

TEST_F(UiTest, VoiceSearchHiddenWhenCantAskForPermission) {
  CreateScene(kNotInCct, kNotInWebVr);

  model_->speech.has_or_can_request_audio_permission = true;
  EXPECT_TRUE(OnBeginFrame());
  EXPECT_TRUE(IsVisible(kVoiceSearchButton));

  model_->speech.has_or_can_request_audio_permission = false;
  EXPECT_TRUE(OnBeginFrame());
  EXPECT_FALSE(IsVisible(kVoiceSearchButton));
}

TEST_F(UiTest, WebVrAutopresented) {
  CreateSceneForAutoPresentation();

  // Initially, we should only show the splash screen.
  VerifyOnlyElementsVisible("Initial",
                            {kSplashScreenText, kSplashScreenBackground});

  // Enter WebVR with autopresentation.
  ui_->SetWebVrMode(true, false);
  ui_->OnWebVrFrameAvailable();

  // The splash screen should go away.
  RunFor(
      MsToDelta(1000 * (kSplashScreenMinDurationSeconds + kSmallDelaySeconds)));
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
  VerifyOnlyElementsVisible("Initial",
                            {kSplashScreenText, kSplashScreenBackground});

  ui_->SetWebVrMode(true, false);
  ui_->OnWebVrTimeoutImminent();

  VerifyOnlyElementsVisible(
      "Timeout imminent",
      {kWebVrTimeoutSpinner, kWebVrTimeoutSpinnerBackground});
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
  visible_in_fullscreen.insert(kBackplane);
  visible_in_fullscreen.insert(kCloseButton);
  visible_in_fullscreen.insert(kExclusiveScreenToast);
  visible_in_fullscreen.insert(kController);
  visible_in_fullscreen.insert(kLaser);
  visible_in_fullscreen.insert(kReticle);

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
  ui_->SetExitVrPromptEnabled(true, UiUnsupportedMode::kUnhandledPageInfo);

  VerifyOnlyElementsVisible("Prompt visible", kElementsVisibleWithExitPrompt);

  // Click on backplane should trigger UI browser interface but not close
  // prompt.
  EXPECT_CALL(*browser_,
              OnExitVrPromptResult(ExitVrPromptChoice::CHOICE_NONE,
                                   UiUnsupportedMode::kUnhandledPageInfo));
  scene_->GetUiElementByName(kExitPromptBackplane)->OnButtonUp(gfx::PointF());

  // This would usually get called by the browser, but since it is mocked we
  // will call it explicitly here and check that the UI responds as we would
  // expect.
  ui_->SetExitVrPromptEnabled(false, UiUnsupportedMode::kCount);
  VerifyOnlyElementsVisible("Prompt cleared", kElementsVisibleInBrowsing);
}

TEST_F(UiTest, PrimaryButtonClickTriggersOnExitPrompt) {
  CreateScene(kNotInCct, kNotInWebVr);

  // Initial state.
  VerifyOnlyElementsVisible("Initial", kElementsVisibleInBrowsing);
  model_->active_modal_prompt_type = kModalPromptTypeExitVRForSiteInfo;
  OnBeginFrame();

  // Click on 'OK' should trigger UI browser interface but not close prompt.
  EXPECT_CALL(*browser_,
              OnExitVrPromptResult(ExitVrPromptChoice::CHOICE_STAY,
                                   UiUnsupportedMode::kUnhandledPageInfo));
  static_cast<ExitPrompt*>(scene_->GetUiElementByName(kExitPrompt))
      ->ClickPrimaryButtonForTesting();
  VerifyOnlyElementsVisible("Prompt still visible",
                            kElementsVisibleWithExitPrompt);
}

TEST_F(UiTest, SecondaryButtonClickTriggersOnExitPrompt) {
  CreateScene(kNotInCct, kNotInWebVr);

  // Initial state.
  VerifyOnlyElementsVisible("Initial", kElementsVisibleInBrowsing);
  model_->active_modal_prompt_type = kModalPromptTypeExitVRForSiteInfo;
  OnBeginFrame();

  // Click on 'Exit VR' should trigger UI browser interface but not close
  // prompt.
  EXPECT_CALL(*browser_,
              OnExitVrPromptResult(ExitVrPromptChoice::CHOICE_EXIT,
                                   UiUnsupportedMode::kUnhandledPageInfo));

  static_cast<ExitPrompt*>(scene_->GetUiElementByName(kExitPrompt))
      ->ClickSecondaryButtonForTesting();
  VerifyOnlyElementsVisible("Prompt still visible",
                            kElementsVisibleWithExitPrompt);
}

TEST_F(UiTest, UiUpdatesForWebVR) {
  CreateScene(kNotInCct, kInWebVr);

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
  VerifyOnlyElementsVisible("Elements hidden", std::set<UiElementName>{});
}

TEST_F(UiTest, UiUpdateTransitionToWebVR) {
  CreateScene(kNotInCct, kNotInWebVr);
  model_->permissions.audio_capture_enabled = true;
  model_->permissions.video_capture_enabled = true;
  model_->permissions.screen_capture_enabled = true;
  model_->permissions.location_access = true;
  model_->permissions.bluetooth_connected = true;

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

TEST_F(UiTest, HitTestableElements) {
  CreateScene(kNotInCct, kNotInWebVr);
  EXPECT_TRUE(RunFor(MsToDelta(0)));
  CheckHitTestableRecursive(&scene_->root_element());
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
  model_->web_vr_timeout_state = kWebVrAwaitingFirstFrame;

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
          kWebVrTimeoutSpinnerBackground,
      },
      true);

  model_->web_vr_timeout_state = kWebVrTimeoutImminent;
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
          kWebVrTimeoutSpinner, kWebVrTimeoutSpinnerBackground,
      },
      true);

  model_->web_vr_timeout_state = kWebVrTimedOut;
  RunFor(MsToDelta(500));
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

TEST_F(UiTest, SpeechRecognitionUiVisibility) {
  CreateScene(kNotInCct, kNotInWebVr);

  model_->speech.recognizing_speech = true;

  // Start hiding browsing foreground and showing speech recognition listening
  // UI.
  EXPECT_TRUE(RunFor(MsToDelta(10)));
  VerifyIsAnimating({k2dBrowsingForeground, kSpeechRecognitionListening},
                    {OPACITY}, true);
  VerifyIsAnimating({kSpeechRecognitionResult}, {OPACITY}, false);

  EXPECT_TRUE(RunFor(MsToDelta(kSpeechRecognitionOpacityAnimationDurationMs)));
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
  EXPECT_TRUE(RunFor(MsToDelta(10)));
  VerifyIsAnimating({kSpeechRecognitionListeningGrowingCircle}, {CIRCLE_GROW},
                    true);

  // Mock received speech result.
  model_->speech.recognition_result = base::ASCIIToUTF16("test");
  model_->speech.recognizing_speech = false;
  model_->speech.speech_recognition_state = SPEECH_RECOGNITION_END;

  EXPECT_TRUE(RunFor(MsToDelta(10)));
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

  model_->speech.recognizing_speech = true;
  EXPECT_TRUE(RunFor(MsToDelta(kSpeechRecognitionOpacityAnimationDurationMs)));
  VerifyIsAnimating({k2dBrowsingForeground, kSpeechRecognitionListening},
                    {OPACITY}, false);

  // Mock exit without a recognition result
  model_->speech.recognition_result.clear();
  model_->speech.recognizing_speech = false;
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

  model_->omnibox_input_active = true;
  OnBeginFrame();
  EXPECT_EQ(container->children().size(), 0u);
  EXPECT_EQ(NumVisibleInTree(kOmniboxSuggestions), 1);

  model_->omnibox_suggestions.emplace_back(
      OmniboxSuggestion(base::string16(), base::string16(),
                        AutocompleteMatch::Type::VOICE_SUGGEST, GURL()));
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
  model_->omnibox_suggestions.emplace_back(
      OmniboxSuggestion(base::string16(), base::string16(),
                        AutocompleteMatch::Type::VOICE_SUGGEST, gurl));
  OnBeginFrame();

  UiElement* suggestions = scene_->GetUiElementByName(kOmniboxSuggestions);
  ASSERT_NE(suggestions, nullptr);
  UiElement* suggestion = suggestions->children().front().get();
  ASSERT_NE(suggestion, nullptr);
  EXPECT_CALL(*browser_, Navigate(gurl)).Times(1);
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

TEST_F(UiTest, SecondExitPromptTriggersOnExitPrompt) {
  CreateScene(kNotInCct, kNotInWebVr);
  ui_->SetExitVrPromptEnabled(true, UiUnsupportedMode::kUnhandledPageInfo);
  // Initiating another exit VR prompt while a previous one was in flight should
  // result in a call to the UiBrowserInterface.
  EXPECT_CALL(*browser_,
              OnExitVrPromptResult(ExitVrPromptChoice::CHOICE_NONE,
                                   UiUnsupportedMode::kUnhandledPageInfo));
  ui_->SetExitVrPromptEnabled(
      true, UiUnsupportedMode::kVoiceSearchNeedsRecordAudioOsPermission);
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

TEST(UiReticleTest, ReticleRenderedOnPlanarChildren) {
  UiScene scene;
  Model model;

  auto reticle = base::MakeUnique<Reticle>(&scene, &model);
  reticle->set_draw_phase(kPhaseNone);
  scene.root_element().AddChild(std::move(reticle));

  auto element = base::MakeUnique<UiElement>();
  UiElement* parent = element.get();
  parent->set_draw_phase(kPhaseForeground);
  parent->set_name(k2dBrowsingRoot);
  scene.root_element().AddChild(std::move(element));
  model.reticle.target_element_id = parent->id();

  // Add 4 children to the parent:
  // - Parent (hit element, initially having reticle)
  //   - Planar
  //   - Planar but offset (should receive reticle)
  //   - Parallel
  //   - Rotated
  for (int i = 0; i < 4; i++) {
    element = base::MakeUnique<UiElement>();
    element->set_draw_phase(kPhaseForeground);
    parent->AddChild(std::move(element));
  }
  parent->children()[1]->SetTranslate(1, 0, 0);
  parent->children()[2]->SetTranslate(0, 0, -1);
  parent->children()[3]->SetRotate(1, 0, 0, 1);
  scene.OnBeginFrame(MsToTicks(0), kForwardVector);

  // Sorted set should have 1 parent, 4 children and 1 reticle.
  UiScene::Elements sorted = scene.GetVisible2dBrowsingElements();
  EXPECT_EQ(6u, sorted.size());
  // Reticle should be after the parent and first two children.
  EXPECT_EQ(sorted[3]->name(), kReticle);
}

// This test ensures that the render order matches the expected tree state. All
// phases are merged, as the order is the same. Unnamed and unrenderable
// elements are skipped.
TEST_F(UiTest, UiRendererSortingTest) {
  CreateScene(kNotInCct, kNotInWebVr);
  auto unsorted = scene_->GetPotentiallyVisibleElements();
  auto sorted = UiRenderer::GetElementsInDrawOrder(unsorted);
  EXPECT_EQ(kElementsInDrawOrder.size(), sorted.size());
  for (size_t i = 0; i < sorted.size(); ++i) {
    ASSERT_EQ(kElementsInDrawOrder[i], sorted[i]->DebugName());
  }
}

TEST_F(UiTest, ReticleStacking) {
  CreateScene(kNotInCct, kNotInWebVr);
  UiElement* content = scene_->GetUiElementByName(kContentQuad);
  EXPECT_TRUE(content);
  model_->reticle.target_element_id = content->id();
  auto unsorted = scene_->GetVisible2dBrowsingElements();
  auto sorted = UiRenderer::GetElementsInDrawOrder(unsorted);
  bool saw_target = false;
  for (auto* e : sorted) {
    if (e == content) {
      saw_target = true;
    } else if (saw_target) {
      EXPECT_EQ(kReticle, e->name());
      break;
    }
  }

  EXPECT_TRUE(saw_target);

  auto controller_elements = scene_->GetVisibleControllerElements();
  bool saw_reticle = false;
  for (auto* e : controller_elements) {
    if (e->name() == kReticle) {
      saw_reticle = true;
    }
  }
  EXPECT_FALSE(saw_reticle);
}

TEST_F(UiTest, ReticleStackingAtopForeground) {
  CreateScene(kNotInCct, kNotInWebVr);
  UiElement* element = scene_->GetUiElementByName(kContentQuad);
  EXPECT_TRUE(element);
  element->set_draw_phase(kPhaseOverlayForeground);
  model_->reticle.target_element_id = element->id();
  auto unsorted = scene_->GetVisible2dBrowsingOverlayElements();
  auto sorted = UiRenderer::GetElementsInDrawOrder(unsorted);
  bool saw_target = false;
  for (auto* e : sorted) {
    if (e == element) {
      saw_target = true;
    } else if (saw_target) {
      EXPECT_EQ(kReticle, e->name());
      break;
    }
  }
  EXPECT_TRUE(saw_target);

  auto controller_elements = scene_->GetVisibleControllerElements();
  bool saw_reticle = false;
  for (auto* e : controller_elements) {
    if (e->name() == kReticle) {
      saw_reticle = true;
    }
  }
  EXPECT_FALSE(saw_reticle);
}

TEST_F(UiTest, ReticleStackingWithControllerElements) {
  CreateScene(kNotInCct, kNotInWebVr);
  UiElement* element = scene_->GetUiElementByName(kReticle);
  EXPECT_TRUE(element);
  element->set_draw_phase(kPhaseBackground);
  EXPECT_NE(scene_->GetUiElementByName(kLaser)->draw_phase(),
            element->draw_phase());
  model_->reticle.target_element_id = 0;
  auto unsorted = scene_->GetVisibleControllerElements();
  auto sorted = UiRenderer::GetElementsInDrawOrder(unsorted);
  EXPECT_EQ(element->DebugName(), sorted.back()->DebugName());
  EXPECT_EQ(scene_->GetUiElementByName(kLaser)->draw_phase(),
            element->draw_phase());
}

// Tests that transient elements will show, even if there is a long delay
// between when they are asked to appear and when we issue the first frame with
// them visible.
TEST_F(UiTest, TransientToastsWithDelayedFirstFrame) {
  CreateSceneForAutoPresentation();

  // Initially, we should only show the splash screen.
  VerifyOnlyElementsVisible("Initial",
                            {kSplashScreenText, kSplashScreenBackground});

  // Enter WebVR with autopresentation.
  ui_->SetWebVrMode(true, false);
  EXPECT_TRUE(RunFor(MsToDelta(2000)));
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

}  // namespace vr
