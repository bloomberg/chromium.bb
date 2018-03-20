// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_UI_SCENE_CONSTANTS_H_
#define CHROME_BROWSER_VR_UI_SCENE_CONSTANTS_H_

#include "ui/gfx/geometry/angle_conversions.h"

namespace vr {

static constexpr int kWarningTimeoutSeconds = 30;
static constexpr float kWarningDistance = 1.0f;
static constexpr float kWarningAngleRadians = gfx::DegToRad(16.3f);
static constexpr float kPermanentWarningHeightDMM = 0.049f;
static constexpr float kPermanentWarningWidthDMM = 0.1568f;
static constexpr float kTransientWarningHeightDMM = 0.160f;
static constexpr float kTransientWarningWidthDMM = 0.512f;

static constexpr float kExitWarningDistance = 0.6f;
static constexpr float kExitWarningTextWidthDMM = 0.44288f;
static constexpr float kExitWarningFontHeightDMM = 0.024576f;
static constexpr float kExitWarningXPaddingDMM = 0.033f;
static constexpr float kExitWarningYPaddingDMM = 0.023f;
static constexpr float kExitWarningCornerRadiusDMM = 0.008f;

static constexpr float kContentDistance = 2.5f;
static constexpr float kContentWidthDMM = 0.96f;
static constexpr float kContentHeightDMM = 0.64f;
static constexpr float kContentWidth = kContentWidthDMM * kContentDistance;
static constexpr float kContentHeight = kContentHeightDMM * kContentDistance;
static constexpr float kContentVerticalOffsetDMM = -0.1f;
static constexpr float kContentVerticalOffset =
    kContentVerticalOffsetDMM * kContentDistance;
static constexpr float kContentCornerRadius = 0.005f * kContentWidth;
static constexpr float kContentShadowOffset = 0.09f;
static constexpr float kContentShadowIntesity = 0.4f;
static constexpr float kBackplaneSize = 1000.0f;
static constexpr float kBackgroundDistanceMultiplier = 1.414f;

static constexpr float kFullscreenDistance = 3.0f;
// Make sure that the aspect ratio for fullscreen is 16:9. Otherwise, we may
// experience visual artefacts for fullscreened videos.
static constexpr float kFullscreenHeightDMM = 0.64f;
static constexpr float kFullscreenHeight =
    kFullscreenHeightDMM * kFullscreenDistance;
static constexpr float kFullscreenWidth = 1.138f * kFullscreenDistance;
static constexpr float kFullscreenVerticalOffsetDMM = -0.1f;
static constexpr float kFullscreenVerticalOffset =
    kFullscreenVerticalOffsetDMM * kFullscreenDistance;

static constexpr float kExitPromptWidth = 0.672f * kContentDistance;
static constexpr float kExitPromptHeight = 0.2f * kContentDistance;
static constexpr float kExitPromptVerticalOffset = -0.09f * kContentDistance;

static constexpr float kUrlBarDistance = 2.4f;
static constexpr float kUrlBarWidthDMM = 0.672f;
static constexpr float kUrlBarHeightDMM = 0.088f;
// This is the non-DMM relative offset of the URL bar. It is used to position
// the DMM root of the URL bar.
static constexpr float kUrlBarRelativeOffset = -0.45f;
// This is the absolute offset of the URL bar's neutral position in DMM.
static constexpr float kUrlBarVerticalOffsetDMM = -0.516f;
static constexpr float kUrlBarRotationRad = gfx::DegToRad(-10.0f);
static constexpr float kUrlBarFontHeightDMM = 0.027f;
static constexpr float kUrlBarButtonSizeDMM = 0.064f;
static constexpr float kUrlBarButtonIconSizeDMM = 0.038f;
static constexpr float kUrlBarEndButtonIconOffsetDMM = 0.0045f;
static constexpr float kUrlBarEndButtonWidthDMM = 0.088f;
static constexpr float kUrlBarSeparatorWidthDMM = 0.002f;
static constexpr float kUrlBarOriginRegionWidthDMM = 0.492f;
static constexpr float kUrlBarOriginContentWidthDMM = 0.452f;
static constexpr float kUrlBarOriginContentOffsetDMM = 0.020f;
static constexpr float kUrlBarFieldSpacingDMM = 0.014f;
static constexpr float kUrlBarOfflineIconTextSpacingDMM = 0.004f;
static constexpr float kUrlBarSecuritySeparatorHeightDMM = 0.026f;
static constexpr float kUrlBarOriginFadeWidth = 0.044f;
static constexpr float kUrlBarOriginMinimumPathWidth = 0.044f;
static constexpr float kUrlBarBackplaneTopPadding = 0.065f;
static constexpr float kUrlBarBackplanePadding = 0.005f;
static constexpr float kUrlBarItemCornerRadiusDMM = 0.006f;
static constexpr float kUrlBarButtonIconScaleFactor =
    kUrlBarButtonIconSizeDMM / kUrlBarButtonSizeDMM;

static constexpr float kOverlayPlaneDistance = 2.3f;

static constexpr float kAudioPermissionPromptWidth = 0.63f * kUrlBarDistance;
static constexpr float kAudioPermissionPromptHeight = 0.218f * kUrlBarDistance;
static constexpr float kAudionPermisionPromptDepth = 0.11f;

static constexpr float kIndicatorHeight = 0.08f;
static constexpr float kIndicatorXPadding = kIndicatorHeight * 0.1f;
static constexpr float kIndicatorYPadding = kIndicatorHeight * 0.15f;
static constexpr float kIndicatorIconSize = kIndicatorHeight * 0.7f;
static constexpr float kIndicatorCornerRadius = kIndicatorHeight * 0.1f;
static constexpr float kIndicatorMargin = kIndicatorHeight * 0.2f;
static constexpr float kIndicatorFontHeightDmm = 0.032f;
static constexpr float kIndicatorGap = 0.05f;
static constexpr float kIndicatorVerticalOffset = 0.1f;
static constexpr float kIndicatorDistanceOffset = 0.1f;

static constexpr float kWebVrUrlToastWidthDMM = 0.472f;
static constexpr float kWebVrUrlToastHeightDMM = 0.064f;
static constexpr float kWebVrUrlToastDistance = 1.0f;
static constexpr float kWebVrUrlToastWidth =
    kWebVrUrlToastWidthDMM * kWebVrUrlToastDistance;
static constexpr float kWebVrUrlToastHeight =
    kWebVrUrlToastHeightDMM * kWebVrUrlToastDistance;
static constexpr int kWebVrUrlToastTimeoutSeconds = 6;
static constexpr float kWebVrUrlToastOpacity = 0.8f;
static constexpr float kWebVrUrlToastRotationRad = gfx::DegToRad(14.0f);

static constexpr float kWebVrToastDistance = 1.0f;
static constexpr float kToastWidthDMM = 0.512f;
static constexpr float kToastHeightDMM = 0.064f;
static constexpr float kToastOffsetDMM = 0.004f;
static constexpr float kFullScreenToastOffsetDMM = 0.1f;
static constexpr float kExclusiveScreenToastXPaddingDMM = 0.017f;
static constexpr float kExclusiveScreenToastYPaddingDMM = 0.02f;
static constexpr float kExclusiveScreenToastCornerRadiusDMM = 0.004f;
static constexpr float kExclusiveScreenToastTextFontHeightDMM = 0.023f;
// When changing the value here, make sure it doesn't collide with
// kWarningAngleRadians.
static constexpr float kWebVrAngleRadians = gfx::DegToRad(9.88f);
static constexpr int kToastTimeoutSeconds = kWebVrUrlToastTimeoutSeconds;

static constexpr float kSplashScreenTextDistance = 2.5f;
static constexpr float kSplashScreenTextFontHeightDMM = 0.05f;
static constexpr float kSplashScreenTextWidthDMM = 0.9f;
static constexpr float kSplashScreenTextVerticalOffsetDMM = -0.072f;
static constexpr float kSplashScreenMinDurationSeconds = 2.0f;

static constexpr float kButtonDiameterDMM = 0.088f;
static constexpr float kButtonZOffsetHoverDMM = 0.048f;

static constexpr float kCloseButtonDistance = 2.4f;
static constexpr float kCloseButtonRelativeOffset = -0.8f;
static constexpr float kCloseButtonVerticalOffset =
    kFullscreenVerticalOffset - (kFullscreenHeight * 0.5f) - 0.35f;
static constexpr float kCloseButtonDiameter =
    kButtonDiameterDMM * kCloseButtonDistance;
static constexpr float kCloseButtonFullscreenDistance = 2.9f;
static constexpr float kCloseButtonFullscreenVerticalOffset =
    kFullscreenVerticalOffset - (kFullscreenHeight / 2) - 0.35f;
static constexpr float kCloseButtonFullscreenDiameter =
    kButtonDiameterDMM * kCloseButtonFullscreenDistance;

static constexpr float kLoadingIndicatorWidthDMM = 0.24f;
static constexpr float kLoadingIndicatorHeightDMM = 0.008f;
static constexpr float kLoadingIndicatorVerticalOffsetDMM =
    (-kUrlBarVerticalOffsetDMM + kContentVerticalOffsetDMM -
     kContentHeightDMM / 2 - kUrlBarHeightDMM / 2) /
    2;
static constexpr float kLoadingIndicatorDepthOffset =
    (kUrlBarDistance - kContentDistance) / 2;

static constexpr float kSceneSize = 25.0f;
static constexpr float kSceneHeight = 4.0f;
static constexpr int kFloorGridlineCount = 40;

static constexpr float kVoiceSearchCloseButtonDiameterDMM = 0.096f;
static constexpr float kVoiceSearchCloseButtonDiameter =
    kVoiceSearchCloseButtonDiameterDMM * kContentDistance;
static constexpr float kVoiceSearchCloseButtonYOffset =
    0.316f * kContentDistance + 0.5f * kVoiceSearchCloseButtonDiameter;
static constexpr float kVoiceSearchRecognitionResultTextHeight =
    0.026f * kContentDistance;
static constexpr float kVoiceSearchRecognitionResultTextWidth =
    0.4f * kContentDistance;
static constexpr float kVoiceSearchIconScaleFactor = 0.75f;

static constexpr float kTimeoutScreenDisatance = 2.5f;
static constexpr float kTimeoutSpinnerSizeDMM = 0.088f;
static constexpr float kTimeoutSpinnerVerticalOffsetDMM =
    kSplashScreenTextVerticalOffsetDMM;

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

static constexpr float kHostedUiHeightRatio = 0.6f;
static constexpr float kHostedUiWidthRatio = 0.6f;
static constexpr float kHostedUiDepthOffset = 0.3f;

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
static constexpr float kOmniboxCloseButtonDiameterDMM = kButtonDiameterDMM;
static constexpr float kOmniboxCloseButtonVerticalOffsetDMM = -0.75f;
static constexpr float kOmniboxCornerRadiusDMM = 0.006f;
static constexpr float kOmniboxCloseButtonDepthOffset = -0.35f;
static constexpr float kOmniboxShadowOffset = 0.07f;
static constexpr float kOmniboxShadowIntensity = 0.4f;
static constexpr int kOmniboxTransitionMs = 300;

static constexpr float kOmniboxTextFieldIconSizeDMM = 0.05f;
static constexpr float kOmniboxTextFieldIconButtonSizeDMM = 0.064f;
static constexpr float kOmniboxTextFieldIconButtonHoverOffsetDMM = 0.012f;
static constexpr float kOmniboxTextFieldRightMargin =
    ((kOmniboxHeightDMM - kOmniboxTextFieldIconButtonSizeDMM) / 2);

static constexpr float kSuggestionHeightDMM = 0.088f;
static constexpr float kSuggestionGapDMM = 0.0018f;
static constexpr float kSuggestionLineGapDMM = 0.01f;
static constexpr float kSuggestionIconSizeDMM = 0.036f;
static constexpr float kSuggestionIconFieldWidthDMM = 0.104f;
static constexpr float kSuggestionRightMarginDMM = 0.024f;
static constexpr float kSuggestionTextFieldWidthDMM =
    kOmniboxWidthDMM - kSuggestionIconFieldWidthDMM - kSuggestionRightMarginDMM;
static constexpr float kSuggestionContentTextHeightDMM = 0.024f;
static constexpr float kSuggestionDescriptionTextHeightDMM = 0.020f;
static constexpr float kSuggestionVerticalPaddingDMM = 0.008f;

static constexpr int kControllerFadeInMs = 200;
static constexpr int kControllerFadeOutMs = 550;

static constexpr float kSpeechRecognitionResultTextYOffset = 0.5f;
static constexpr int kSpeechRecognitionResultTimeoutSeconds = 2;
static constexpr int kSpeechRecognitionOpacityAnimationDurationMs = 200;

static constexpr float kModalPromptFadeOpacity = 0.5f;

static constexpr float kKeyboardDistance = 2.2f;
static constexpr float kKeyboardVerticalOffsetDMM = -0.45f;
static constexpr float kKeyboardRotationRadians = -0.14f;

static constexpr float kSnackbarDistance = 1.5f;
static constexpr float kSnackbarAngle = -gfx::DegToRad(34.0f);
static constexpr float kSnackbarPaddingDMM = 0.032f;
static constexpr float kSnackbarIconWidthDMM = 0.034f;
static constexpr float kSnackbarFontHeightDMM = 0.024f;
static constexpr float kSnackbarHeightDMM = 0.08f;
static constexpr float kSnackbarMoveInAngle = -base::kPiFloat / 10;
static constexpr int kSnackbarTransitionDurationMs = 300;

static constexpr float kControllerLabelSpacerSize = 0.025f;
static constexpr float kControllerLabelLayoutMargin = -0.005f;
static constexpr float kControllerLabelCalloutWidth = 0.02f;
static constexpr float kControllerLabelCalloutHeight = 0.001f;
static constexpr float kControllerLabelFontHeight = 0.05f;
static constexpr float kControllerLabelScale = 0.2f;

// TODO(vollick): these should be encoded in the controller mesh.
static constexpr float kControllerTrackpadOffset = -0.035f;
static constexpr float kControllerExitButtonOffset = -0.008f;
static constexpr float kControllerBackButtonOffset = -0.008f;

static constexpr int kControllerLabelTransitionDurationMs = 700;

static constexpr float kControllerWidth = 0.035f;
static constexpr float kControllerHeight = 0.016f;
static constexpr float kControllerLength = 0.105f;
static constexpr float kControllerSmallButtonSize = kControllerWidth * 0.306f;
static constexpr float kControllerAppButtonZ = kControllerLength * -0.075f;
static constexpr float kControllerHomeButtonZ = kControllerLength * 0.075f;

static constexpr float kSkyDistance = 1000.0f;
static constexpr float kGridOpacity = 0.5f;

static constexpr float kRepositionContentOpacity = 0.2f;

static constexpr float kPromptWidthDMM = 0.63f;
static constexpr float kPromptHeightDMM = 0.218f;
static constexpr float kPromptVerticalOffsetDMM = -0.1f;
static constexpr float kPromptShadowOffsetDMM = 0.1f;
static constexpr float kPromptDistance = 2.4f;

static constexpr float kRepositionCursorBackgroundSize = 1.85f;
static constexpr float kRepositionCursorSize = 1.5f;

static constexpr float kMinResizerScale = 0.5f;
static constexpr float kMaxResizerScale = 1.5f;

static constexpr float kRepositionFrameTolerance = 0.3f;
static constexpr float kRepositionFrameTopPadding = 0.25f;
static constexpr float kRepositionFrameEdgePadding = 0.04f;
static constexpr float kRepositionFrameMaxOpacity = 0.6f;
static constexpr float kRepositionFrameHitPlaneTopPadding = 0.5f;
static constexpr float kRepositionFrameTransitionDurationMs = 300;

static constexpr float kOverflowMenuOffset = 0.016f;
static constexpr float kOverflowMenuMinimumWidth = 0.312f;
static constexpr float kOverflowButtonRegionHeight = 0.088f;
static constexpr float kOverflowButtonXOffset = 0.016f;
static constexpr float kOverflowMenuYPadding = 0.012f;
static constexpr float kOverflowMenuItemHeight = 0.080f;
static constexpr float kOverflowMenuItemXPadding = 0.024f;

}  // namespace vr

#endif  // CHROME_BROWSER_VR_UI_SCENE_CONSTANTS_H_
