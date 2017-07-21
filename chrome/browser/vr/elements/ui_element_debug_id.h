// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_UI_ELEMENT_DEBUG_ID_H_
#define CHROME_BROWSER_VR_ELEMENTS_UI_ELEMENT_DEBUG_ID_H_

namespace vr {

// These identifiers may be used by UI elements to tag themselves for purposes
// of testing or debugging.
enum UiElementDebugId {
  kNone = 0,
  kWebVrPermanentHttpSecurityWarning,
  kWebVrTransientHttpSecurityWarning,
  kContentQuad,
  kBackplane,
  kCeiling,
  kFloor,
  kUrlBar,
  kLoadingIndicator,
  kAudioCaptureIndicator,
  kVideoCaptureIndicator,
  kScreenCaptureIndicator,
  kCloseButton,
  kScreenDimmer,
  kExitWarning,
  kExitPrompt,
  kExitPromptBackplane,
  kTransientUrlBar,
  kLocationAccessIndicator,
  kExclusiveScreenToast,
  kSplashScreenIcon,
  kBluetoothConnectedIndicator,
  kBackgroundFront,
  kBackgroundLeft,
  kBackgroundBack,
  kBackgroundRight,
  kBackgroundTop,
  kBackgroundBottom,
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_UI_ELEMENT_DEBUG_ID_H_
