// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_DOM_LOGIN_DISPLAY_HOST_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_DOM_LOGIN_DISPLAY_HOST_H_
#pragma once

#include <string>

#include "chrome/browser/chromeos/login/base_login_display_host.h"
#include "ui/gfx/rect.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace chromeos {

// DOM-specific implementation of the OOBE/login screen host.
// Uses DOMLoginDisplay as the login screen UI implementation,
class DOMLoginDisplayHost : public BaseLoginDisplayHost {
 public:
  explicit DOMLoginDisplayHost(const gfx::Rect& background_bounds);
  virtual ~DOMLoginDisplayHost();

  // LoginDisplayHost implementation:
  virtual LoginDisplay* CreateLoginDisplay(LoginDisplay::Delegate* delegate)
      const OVERRIDE;
  virtual gfx::NativeWindow GetNativeWindow() const OVERRIDE;
  virtual void SetOobeProgress(BackgroundView::LoginStep step) OVERRIDE;
  virtual void SetOobeProgressBarVisible(bool visible) OVERRIDE;
  virtual void SetShutdownButtonEnabled(bool enable) OVERRIDE;
  virtual void SetStatusAreaEnabled(bool enable) OVERRIDE;
  virtual void SetStatusAreaVisible(bool visible) OVERRIDE;
  virtual void ShowBackground() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DOMLoginDisplayHost);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_DOM_LOGIN_DISPLAY_HOST_H_
