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
static constexpr float kExitPromptBackplaneSize = 1000.0;

// Distance-independent milimeter size of the URL bar.
static constexpr float kUrlBarWidthDMM = 0.672f;
static constexpr float kUrlBarHeightDMM = 0.088f;
static constexpr float kUrlBarDistance = 2.4f;
static constexpr float kUrlBarWidth = kUrlBarWidthDMM * kUrlBarDistance;
static constexpr float kUrlBarHeight = kUrlBarHeightDMM * kUrlBarDistance;
static constexpr float kUrlBarVerticalOffset = -0.516f * kUrlBarDistance;
static constexpr float kUrlBarRotationRad = -0.175f;

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

static constexpr float kCloseButtonDistance = 2.4f;
static constexpr float kCloseButtonHeight =
    kUrlBarHeightDMM * kCloseButtonDistance;
static constexpr float kCloseButtonWidth =
    kUrlBarHeightDMM * kCloseButtonDistance;
static constexpr float kCloseButtonFullscreenDistance = 2.9f;
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

// Tiny distance to offset textures that should appear in the same plane.
static constexpr float kTextureOffset = 0.01f;

static constexpr float kUnderDevelopmentNoticeFontHeightM =
    0.02f * kUrlBarDistance;
static constexpr float kUnderDevelopmentNoticeHeightM = 0.1f * kUrlBarDistance;
static constexpr float kUnderDevelopmentNoticeWidthM = 0.44f * kUrlBarDistance;
static constexpr float kUnderDevelopmentNoticeVerticalOffsetM =
    0.5f * kUnderDevelopmentNoticeHeightM + kUrlBarHeight;
static constexpr float kUnderDevelopmentNoticeRotationRad = -0.19f;

// If the screen space bounds or the aspect ratio of the content quad change
// beyond these thresholds we propagate the new content bounds so that the
// content's resolution can be adjusted.
static constexpr float kContentBoundsPropagationThreshold = 0.2f;
// Changes of the aspect ratio lead to a
// distorted content much more quickly. Thus, have a smaller threshold here.
static constexpr float kContentAspectRatioPropagationThreshold = 0.01f;

static constexpr float kScreenDimmerOpacity = 0.9f;

}  // namespace vr

#endif  // CHROME_BROWSER_VR_UI_SCENE_CONSTANTS_H_
