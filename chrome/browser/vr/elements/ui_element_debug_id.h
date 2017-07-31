// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_UI_ELEMENT_DEBUG_ID_H_
#define CHROME_BROWSER_VR_ELEMENTS_UI_ELEMENT_DEBUG_ID_H_

namespace vr {

// These identifiers serve as stable, semantic identifiers for UI elements.
//
// TODO(vollick): This should become UiElementName. These identifiers will be
// useful outside of testing and debugging code. Named as it is today, it sounds
// as if this can be done away with in release builds, which is not true.
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
