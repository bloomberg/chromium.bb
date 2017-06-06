// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_UI_BROWSER_INTERFACE_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_UI_BROWSER_INTERFACE_H_

#include "chrome/browser/android/vr_shell/ui_unsupported_mode.h"

namespace vr_shell {

// An interface for the UI to communicate with VrShell.  Many of the functions
// in this interface are proxies to methods on VrShell.
class UiBrowserInterface {
 public:
  virtual ~UiBrowserInterface() = default;

  virtual void ExitPresent() = 0;
  virtual void ExitFullscreen() = 0;
  virtual void NavigateBack() = 0;
  virtual void ExitCct() = 0;
  virtual void OnUnsupportedMode(UiUnsupportedMode mode) = 0;
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_UI_BROWSER_INTERFACE_H_
