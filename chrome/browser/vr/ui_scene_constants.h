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
static constexpr float kContentWidth = 0.96f * kContentDistance;
static constexpr float kContentHeight = 0.64f * kContentDistance;
static constexpr float kContentVerticalOffset = -0.1f * kContentDistance;
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

// Distance-independent milimeter size of the URL bar.
static constexpr float kUrlBarWidthDMM = 0.672f;
static constexpr float kUrlBarHeightDMM = 0.088f;
static constexpr float kUrlBarDistance = 2.4f;
static constexpr float kUrlBarWidth = kUrlBarWidthDMM * kUrlBarDistance;
static constexpr float kUrlBarHeight = kUrlBarHeightDMM * kUrlBarDistance;
static constexpr float kUrlBarVerticalOffset = -0.516f * kUrlBarDistance;
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

static constexpr float kButtonZOffsetHoverDMM = 0.048;

static constexpr float kCloseButtonDistance = 2.4f;
static constexpr float kCloseButtonVerticalOffset =
    kFullscreenVerticalOffset - (kFullscreenHeight * 0.5f) - 0.35f;
static constexpr float kCloseButtonHeight =
    kUrlBarHeightDMM * kCloseButtonDistance;
static constexpr float kCloseButtonWidth =
    kUrlBarHeightDMM * kCloseButtonDistance;
static constexpr float kCloseButtonFullscreenDistance = 2.9f;
static constexpr float kCloseButtonFullscreenVerticalOffset =
    kFullscreenVerticalOffset - (kFullscreenHeight / 2) - 0.35f;
static constexpr float kCloseButtonFullscreenHeight =
    kUrlBarHeightDMM * kCloseButtonFullscreenDistance;
static constexpr float kCloseButtonFullscreenWidth =
    kUrlBarHeightDMM * kCloseButtonFullscreenDistance;

static constexpr float kLoadingIndicatorWidth = 0.24f * kUrlBarDistance;
static constexpr float kLoadingIndicatorHeight = 0.008f * kUrlBarDistance;
static constexpr float kLoadingIndicatorVerticalOffset =
    (-kUrlBarVerticalOffset + kContentVerticalOffset - kContentHeight / 2 -
     kUrlBarHeight / 2) /
    2;
static constexpr float kLoadingIndicatorDepthOffset =
    (kUrlBarDistance - kContentDistance) / 2;

static constexpr float kSceneSize = 25.0;
static constexpr float kSceneHeight = 4.0;
static constexpr int kFloorGridlineCount = 40;

static constexpr float kVoiceSearchUIGroupButtonDMM = 0.096f;
static constexpr float kVoiceSearchButtonWidth =
    kVoiceSearchUIGroupButtonDMM * kUrlBarDistance;
static constexpr float kVoiceSearchButtonHeight = kVoiceSearchButtonWidth;
static constexpr float kVoiceSearchButtonYOffset =
    (0.5f * kUrlBarHeightDMM + 0.032f) * kUrlBarDistance;
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

static constexpr float kUnderDevelopmentNoticeFontHeightM =
    0.02f * kUrlBarDistance;
static constexpr float kUnderDevelopmentNoticeHeightM = 0.1f * kUrlBarDistance;
static constexpr float kUnderDevelopmentNoticeWidthM = 0.44f * kUrlBarDistance;
static constexpr float kUnderDevelopmentNoticeVerticalOffsetM =
    kVoiceSearchButtonYOffset + kVoiceSearchButtonHeight +
    0.04f * kUrlBarDistance;
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

static constexpr float kTimeoutButtonVerticalOffset = kUrlBarVerticalOffset;
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

static constexpr float kOmniboxContainerWidth = 4.5;
static constexpr float kOmniboxContainerHeight = 12 * 0.02f;
static constexpr float kOmniboxContainerCornerRadius = 0.01;
static constexpr float kOmniboxContainerVeriticalOffset = 0.7;
static constexpr float kOmniboxTextHeight = 0.05;
static constexpr float kOmniboxCloseButtonVerticalOffset = -0.5;
static constexpr float kSuggestionGap = 0.01f;
static constexpr float kSuggestionLineGap = 0.01f;
static constexpr float kSuggestionIconGap = 0.01f;
static constexpr float kSuggestionIconSize = 0.1f;
static constexpr float kSuggestionTextFieldWidth = 0.3f;
static constexpr float kSuggestionContentTextHeight = 0.02f;
static constexpr float kSuggestionDescriptionTextHeight = 0.015f;

static constexpr int kControllerFadeInMs = 200;
static constexpr int kControllerFadeOutMs = 550;

static constexpr float kSpeechRecognitionResultTextYOffset = 0.5f;
static constexpr int kSpeechRecognitionResultTimeoutSeconds = 2;
static constexpr int kSpeechRecognitionOpacityAnimationDurationMs = 200;

static constexpr float kModalPromptFadeOpacity = 0.5f;

}  // namespace vr

#endif  // CHROME_BROWSER_VR_UI_SCENE_CONSTANTS_H_
