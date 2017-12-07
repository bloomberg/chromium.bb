// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_UI_SCENE_CONSTANTS_H_
#define CHROME_BROWSER_VR_UI_SCENE_CONSTANTS_H_

#include "base/numerics/math_constants.h"

namespace vr {

static constexpr int kWarningTimeoutSeconds = 30;
static constexpr float kWarningDistance = 1.0;
static constexpr float kWarningAngleRadians = 16.3f * base::kPiFloat / 180;
static constexpr float kPermanentWarningHeightDMM = 0.049f;
static constexpr float kPermanentWarningWidthDMM = 0.1568f;
static constexpr float kTransientWarningHeightDMM = 0.160f;
static constexpr float kTransientWarningWidthDMM = 0.512f;

static constexpr float kExitWarningDistance = 0.6f;
static constexpr float kExitWarningHeight = 0.160f;
static constexpr float kExitWarningWidth = 0.512f;

static constexpr float kContentDistance = 2.5;
static constexpr float kContentWidthDMM = 0.96f;
static constexpr float kContentHeightDMM = 0.64f;
static constexpr float kContentWidth = kContentWidthDMM * kContentDistance;
static constexpr float kContentHeight = kContentHeightDMM * kContentDistance;
static constexpr float kContentVerticalOffsetDMM = -0.1f;
static constexpr float kContentVerticalOffset =
    kContentVerticalOffsetDMM * kContentDistance;
static constexpr float kContentCornerRadius = 0.005f * kContentWidth;
static constexpr float kBackplaneSize = 1000.0;
static constexpr float kBackgroundDistanceMultiplier = 1.414f;

static constexpr float kFullscreenDistance = 3;
// Make sure that the aspect ratio for fullscreen is 16:9. Otherwise, we may
// experience visual artefacts for fullscreened videos.
static constexpr float kFullscreenHeight = 0.64f * kFullscreenDistance;
static constexpr float kFullscreenWidth = 1.138f * kFullscreenDistance;
static constexpr float kFullscreenVerticalOffset = -0.1f * kFullscreenDistance;

static constexpr float kExitPromptWidth = 0.672f * kContentDistance;
static constexpr float kExitPromptHeight = 0.2f * kContentDistance;

static constexpr float kExitPromptVerticalOffset = -0.09f * kContentDistance;
static constexpr float kPromptBackplaneSize = 1000.0;

static constexpr float kUrlBarDistance = 2.4f;
static constexpr float kUrlBarWidthDMM = 0.672f;
static constexpr float kUrlBarHeightDMM = 0.088f;
static constexpr float kUrlBarVerticalOffsetDMM = -0.516f;
static constexpr float kUrlBarRotationRad = -0.175f;

static constexpr float kOverlayPlaneDistance = 2.3f;

static constexpr float kAudioPermissionPromptWidth = 0.63f * kUrlBarDistance;
static constexpr float kAudioPermissionPromptHeight = 0.218f * kUrlBarDistance;
static constexpr float kAudionPermisionPromptDepth = 0.11f;

static constexpr float kIndicatorHeight = 0.08f;
static constexpr float kIndicatorGap = 0.05f;
static constexpr float kIndicatorVerticalOffset = 0.1f;
static constexpr float kIndicatorDistanceOffset = 0.1f;

static constexpr float kWebVrUrlToastWidthDMM = 0.472f;
static constexpr float kWebVrUrlToastHeightDMM = 0.064f;
static constexpr float kWebVrUrlToastDistance = 1.0;
static constexpr float kWebVrUrlToastWidth =
    kWebVrUrlToastWidthDMM * kWebVrUrlToastDistance;
static constexpr float kWebVrUrlToastHeight =
    kWebVrUrlToastHeightDMM * kWebVrUrlToastDistance;
static constexpr int kWebVrUrlToastTimeoutSeconds = 6;
static constexpr float kWebVrUrlToastRotationRad = 14 * base::kPiFloat / 180;

static constexpr float kWebVrToastDistance = 1.0;
static constexpr float kFullscreenToastDistance = kFullscreenDistance;
static constexpr float kToastWidthDMM = 0.512f;
static constexpr float kToastHeightDMM = 0.064f;
static constexpr float kToastOffsetDMM = 0.004f;
// When changing the value here, make sure it doesn't collide with
// kWarningAngleRadians.
static constexpr float kWebVrAngleRadians = 9.88f * base::kPiFloat / 180;
static constexpr int kToastTimeoutSeconds = kWebVrUrlToastTimeoutSeconds;

static constexpr float kSplashScreenTextDistance = 2.5f;
static constexpr float kSplashScreenTextFontHeightM =
    0.05f * kSplashScreenTextDistance;
static constexpr float kSplashScreenTextWidthM =
    0.9f * kSplashScreenTextDistance;
static constexpr float kSplashScreenTextHeightM =
    0.08f * kSplashScreenTextDistance;
static constexpr float kSplashScreenTextVerticalOffset = -0.18f;
static constexpr float kSplashScreenMinDurationSeconds = 3;

static constexpr float kButtonDiameterDMM = 0.088f;
static constexpr float kButtonZOffsetHoverDMM = 0.048;

static constexpr float kCloseButtonDistance = 2.4f;
static constexpr float kCloseButtonVerticalOffset =
    kFullscreenVerticalOffset - (kFullscreenHeight * 0.5f) - 0.35f;
static constexpr float kCloseButtonHeightDMM = kButtonDiameterDMM;
static constexpr float kCloseButtonHeight =
    kCloseButtonHeightDMM * kCloseButtonDistance;
static constexpr float kCloseButtonWidth =
    kCloseButtonHeightDMM * kCloseButtonDistance;
static constexpr float kCloseButtonFullscreenDistance = 2.9f;
static constexpr float kCloseButtonFullscreenVerticalOffset =
    kFullscreenVerticalOffset - (kFullscreenHeight / 2) - 0.35f;
static constexpr float kCloseButtonFullscreenHeight =
    kCloseButtonHeightDMM * kCloseButtonFullscreenDistance;
static constexpr float kCloseButtonFullscreenWidth =
    kCloseButtonHeightDMM * kCloseButtonFullscreenDistance;

static constexpr float kLoadingIndicatorWidthDMM = 0.24f;
static constexpr float kLoadingIndicatorHeightDMM = 0.008f;
static constexpr float kLoadingIndicatorVerticalOffsetDMM =
    (-kUrlBarVerticalOffsetDMM + kContentVerticalOffsetDMM -
     kContentHeightDMM / 2 - kUrlBarHeightDMM / 2) /
    2;
static constexpr float kLoadingIndicatorDepthOffset =
    (kUrlBarDistance - kContentDistance) / 2;

static constexpr float kSceneSize = 25.0;
static constexpr float kSceneHeight = 4.0;
static constexpr int kFloorGridlineCount = 40;

static constexpr float kVoiceSearchUIGroupButtonDMM = 0.096f;
static constexpr float kVoiceSearchButtonDiameterDMM =
    kVoiceSearchUIGroupButtonDMM;
static constexpr float kVoiceSearchButtonYOffsetDMM = 0.032f;
static constexpr float kVoiceSearchCloseButtonWidth =
    kVoiceSearchUIGroupButtonDMM * kContentDistance;
static constexpr float kVoiceSearchCloseButtonHeight =
    kVoiceSearchCloseButtonWidth;
static constexpr float kVoiceSearchCloseButtonYOffset =
    0.316f * kContentDistance + 0.5f * kVoiceSearchCloseButtonWidth;
static constexpr float kVoiceSearchRecognitionResultTextHeight =
    0.026f * kContentDistance;
static constexpr float kVoiceSearchRecognitionResultTextWidth =
    0.4f * kContentDistance;

static constexpr float kUnderDevelopmentNoticeFontHeightDMM = 0.02f;
static constexpr float kUnderDevelopmentNoticeHeightDMM = 0.1f;
static constexpr float kUnderDevelopmentNoticeWidthDMM = 0.44f;
static constexpr float kUnderDevelopmentNoticeVerticalOffsetDMM =
    kVoiceSearchButtonYOffsetDMM + kVoiceSearchButtonDiameterDMM * 1.5f + 0.04f;
static constexpr float kUnderDevelopmentNoticeRotationRad = -0.78f;

static constexpr float kSpinnerWidth = kCloseButtonWidth;
static constexpr float kSpinnerHeight = kCloseButtonHeight;
static constexpr float kSpinnerVerticalOffset = kSplashScreenTextVerticalOffset;
static constexpr float kSpinnerDistance = kSplashScreenTextDistance;

static constexpr float kTimeoutMessageHorizontalPaddingDMM = 0.04f;
static constexpr float kTimeoutMessageVerticalPaddingDMM = 0.024f;

static constexpr float kTimeoutMessageCornerRadiusDMM = 0.008f;

static constexpr float kTimeoutMessageLayoutGapDMM = 0.024f;
static constexpr float kTimeoutMessageIconWidthDMM = 0.056f;
static constexpr float kTimeoutMessageIconHeightDMM = 0.056f;
static constexpr float kTimeoutMessageTextFontHeightDMM = 0.022f;
static constexpr float kTimeoutMessageTextHeightDMM = 0.056f;
static constexpr float kTimeoutMessageTextWidthDMM = 0.4f;

static constexpr float kTimeoutButtonVerticalOffset =
    kUrlBarVerticalOffsetDMM * kUrlBarDistance;
static constexpr float kTimeoutButtonDistance = kUrlBarDistance;
static constexpr float kTimeoutButtonDepthOffset = -0.1f;
static constexpr float kTimeoutButtonRotationRad = kUrlBarRotationRad;
static constexpr float kWebVrTimeoutMessageButtonDiameterDMM = 0.096f;

static constexpr float kTimeoutButtonTextWidthDMM = 0.058f;
static constexpr float kTimeoutButtonTextHeightDMM = 0.024f;
static constexpr float kTimeoutButtonTextVerticalOffsetDMM = 0.024f;

static constexpr float kScreenDimmerOpacity = 0.9f;

static constexpr gfx::Point3F kOrigin = {0.0f, 0.0f, 0.0f};

static constexpr float kLaserWidth = 0.01f;

static constexpr float kReticleWidth = 0.025f;
static constexpr float kReticleHeight = 0.025f;

static constexpr float kOmniboxWidthDMM = 0.848f;
static constexpr float kOmniboxHeightDMM = 0.088f;
static constexpr float kOmniboxVerticalOffsetDMM = -0.2f;
static constexpr float kOmniboxTextHeightDMM = 0.032f;
static constexpr float kOmniboxTextMarginDMM = 0.024f;
static constexpr float kOmniboxCloseButtonDiameterDMM = 0.088f;
static constexpr float kOmniboxCloseButtonVerticalOffsetDMM = -0.55;

static constexpr float kSuggestionHeightDMM = 0.088f;
static constexpr float kSuggestionGapDMM = 0.008f;
static constexpr float kSuggestionLineGapDMM = 0.01f;
static constexpr float kSuggestionIconSizeDMM = 0.036f;
static constexpr float kSuggestionIconFieldWidthDMM = 0.104f;
static constexpr float kSuggestionRightMarginDMM = 0.024f;
static constexpr float kSuggestionTextFieldWidthDMM =
    kOmniboxWidthDMM - kSuggestionIconFieldWidthDMM - kSuggestionRightMarginDMM;
static constexpr float kSuggestionContentTextHeightDMM = 0.024f;
static constexpr float kSuggestionDescriptionTextHeightDMM = 0.020f;

static constexpr int kControllerFadeInMs = 200;
static constexpr int kControllerFadeOutMs = 550;

static constexpr float kSpeechRecognitionResultTextYOffset = 0.5f;
static constexpr int kSpeechRecognitionResultTimeoutSeconds = 2;
static constexpr int kSpeechRecognitionOpacityAnimationDurationMs = 200;

static constexpr float kModalPromptFadeOpacity = 0.5f;

static constexpr float kKeyboardDistance = 1.0f;
static constexpr float kKeyboardVerticalOffset = -0.45f * kKeyboardDistance;

}  // namespace vr

#endif  // CHROME_BROWSER_VR_UI_SCENE_CONSTANTS_H_
