// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_UI_ELEMENT_NAME_H_
#define CHROME_BROWSER_VR_ELEMENTS_UI_ELEMENT_NAME_H_

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
  kBackplane,
  kCeiling,
  kFloor,
  kUrlBar,
  kIndicatorLayout,
  kAudioCaptureIndicator,
  kVideoCaptureIndicator,
  kScreenCaptureIndicator,
  kLocationAccessIndicator,
  kBluetoothConnectedIndicator,
  kLoadingIndicator,
  kCloseButton,
  kScreenDimmer,
  kExitWarning,
  kExitPrompt,
  kExitPromptBackplane,
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
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_UI_ELEMENT_NAME_H_
