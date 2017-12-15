// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_UI_ELEMENT_NAME_H_
#define CHROME_BROWSER_VR_ELEMENTS_UI_ELEMENT_NAME_H_

#include <string>

namespace vr {

// These identifiers serve as stable, semantic identifiers for UI elements.
enum UiElementName {
  kNone = 0,
  kRoot,
  k2dBrowsingRoot,
  k2dBrowsingBackground,
  k2dBrowsingForeground,
  k2dBrowsingContentGroup,
  k2dBrowsingViewportAwareRoot,
  kWebVrRoot,
  kWebVrViewportAwareRoot,
  kContentQuad,
  kControllerRoot,
  kControllerGroup,
  kLaser,
  kController,
  kReticle,
  kKeyboardDmmRoot,
  kKeyboard,
  kBackplane,
  kCeiling,
  kFloor,
  kUrlBarDmmRoot,
  kUrlBar,
  kOmniboxDmmRoot,
  kOmniboxRoot,
  kOmniboxContainer,
  kOmniboxTextField,
  kOmniboxClearTextFieldButton,
  kOmniboxCloseButton,
  kOmniboxSuggestions,
  kOmniboxSuggestionsOuterLayout,
  kOmniboxOuterLayout,
  kOmniboxShadow,
  k2dBrowsingVisibiltyControlForOmnibox,
  kIndicatorLayout,
  kAudioCaptureIndicator,
  kVideoCaptureIndicator,
  kScreenCaptureIndicator,
  kLocationAccessIndicator,
  kBluetoothConnectedIndicator,
  kLoadingIndicator,
  kLoadingIndicatorForeground,
  kCloseButton,
  kVoiceSearchButton,
  kScreenDimmer,
  kExitWarningText,
  kExitWarningBackground,
  kExitPrompt,
  kExitPromptBackplane,
  kAudioPermissionPrompt,
  kAudioPermissionPromptShadow,
  kAudioPermissionPromptBackplane,
  kWebVrUrlToastTransientParent,
  kWebVrUrlToast,
  kExclusiveScreenToastTransientParent,
  kExclusiveScreenToast,
  kExclusiveScreenToastViewportAwareTransientParent,
  kExclusiveScreenToastViewportAware,
  kSplashScreenRoot,
  kSplashScreenTransientParent,
  kSplashScreenViewportAwareRoot,
  kSplashScreenText,
  kSplashScreenBackground,
  kBackgroundFront,
  kBackgroundLeft,
  kBackgroundBack,
  kBackgroundRight,
  kBackgroundTop,
  kBackgroundBottom,
  kUnderDevelopmentNotice,
  kWebVrTimeoutSpinner,
  kWebVrTimeoutSpinnerBackground,
  kWebVrTimeoutMessage,
  kWebVrTimeoutMessageLayout,
  kWebVrTimeoutMessageIcon,
  kWebVrTimeoutMessageText,
  kWebVrTimeoutMessageButton,
  kWebVrTimeoutMessageButtonText,
  kSpeechRecognitionRoot,
  kSpeechRecognitionResult,
  kSpeechRecognitionResultText,
  kSpeechRecognitionResultCircle,
  kSpeechRecognitionResultMicrophoneIcon,
  kSpeechRecognitionResultBackplane,
  kSpeechRecognitionListening,
  kSpeechRecognitionListeningGrowingCircle,
  kSpeechRecognitionListeningInnerCircle,
  kSpeechRecognitionListeningMicrophoneIcon,
  kSpeechRecognitionListeningCloseButton,

  // This must be last.
  kNumUiElementNames,
};

std::string UiElementNameToString(UiElementName name);

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_UI_ELEMENT_NAME_H_
