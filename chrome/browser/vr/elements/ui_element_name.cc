// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/ui_element_name.h"

#include "base/logging.h"
#include "base/macros.h"

namespace vr {

namespace {

static const char* g_ui_element_name_strings[] = {
    "kNone",
    "kRoot",
    "k2dBrowsingRoot",
    "k2dBrowsingBackground",
    "k2dBrowsingForeground",
    "k2dBrowsingContentGroup",
    "k2dBrowsingViewportAwareRoot",
    "kWebVrRoot",
    "kWebVrViewportAwareRoot",
    "kContentQuad",
    "kControllerRoot",
    "kControllerGroup",
    "kLaser",
    "kController",
    "kReticle",
    "kKeyboardDmmRoot",
    "kKeyboard",
    "kBackplane",
    "kCeiling",
    "kFloor",
    "kUrlBarDmmRoot",
    "kUrlBar",
    "kOmniboxDmmRoot",
    "kOmniboxRoot",
    "kOmniboxContainer",
    "kOmniboxTextField",
    "kOmniboxClearTextFieldButton",
    "kOmniboxCloseButton",
    "kOmniboxSuggestions",
    "kOmniboxSuggestionsOuterLayout",
    "kOmniboxOuterLayout",
    "kOmniboxShadow",
    "k2dBrowsingVisibiltyControlForOmnibox",
    "kIndicatorLayout",
    "kAudioCaptureIndicator",
    "kVideoCaptureIndicator",
    "kScreenCaptureIndicator",
    "kLocationAccessIndicator",
    "kBluetoothConnectedIndicator",
    "kLoadingIndicator",
    "kLoadingIndicatorForeground",
    "kCloseButton",
    "kVoiceSearchButton",
    "kScreenDimmer",
    "kExitWarningText",
    "kExitWarningBackground",
    "kExitPrompt",
    "kExitPromptBackplane",
    "kAudioPermissionPrompt",
    "kAudioPermissionPromptShadow",
    "kAudioPermissionPromptBackplane",
    "kWebVrUrlToastTransientParent",
    "kWebVrUrlToast",
    "kExclusiveScreenToastTransientParent",
    "kExclusiveScreenToast",
    "kExclusiveScreenToastViewportAwareTransientParent",
    "kExclusiveScreenToastViewportAware",
    "kSplashScreenRoot",
    "kSplashScreenTransientParent",
    "kSplashScreenViewportAwareRoot",
    "kSplashScreenText",
    "kSplashScreenBackground",
    "kBackgroundFront",
    "kBackgroundLeft",
    "kBackgroundBack",
    "kBackgroundRight",
    "kBackgroundTop",
    "kBackgroundBottom",
    "kUnderDevelopmentNotice",
    "kWebVrTimeoutSpinner",
    "kWebVrTimeoutSpinnerBackground",
    "kWebVrTimeoutMessage",
    "kWebVrTimeoutMessageLayout",
    "kWebVrTimeoutMessageIcon",
    "kWebVrTimeoutMessageText",
    "kWebVrTimeoutMessageButton",
    "kWebVrTimeoutMessageButtonText",
    "kSpeechRecognitionRoot",
    "kSpeechRecognitionResult",
    "kSpeechRecognitionResultText",
    "kSpeechRecognitionResultCircle",
    "kSpeechRecognitionResultMicrophoneIcon",
    "kSpeechRecognitionResultBackplane",
    "kSpeechRecognitionListening",
    "kSpeechRecognitionListeningGrowingCircle",
    "kSpeechRecognitionListeningInnerCircle",
    "kSpeechRecognitionListeningMicrophoneIcon",
    "kSpeechRecognitionListeningCloseButton",
};

static_assert(
    kNumUiElementNames == arraysize(g_ui_element_name_strings),
    "Mismatch between the kUiElementName enum and the corresponding array "
    "of strings.");

}  // namespace

std::string UiElementNameToString(UiElementName name) {
  DCHECK_GT(kNumUiElementNames, name);
  return g_ui_element_name_strings[name];
}

}  // namespace vr
