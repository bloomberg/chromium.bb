// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_WEBUI_LOGIN_DISPLAY_HOST_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_WEBUI_LOGIN_DISPLAY_HOST_H_
#pragma once

#include <string>

#include "chrome/browser/chromeos/login/base_login_display_host.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace chromeos {

class OobeUI;
class WebUILoginView;

// WebUI-specific implementation of the OOBE/login screen host. Uses
// WebUILoginDisplay as the login screen UI implementation,
class WebUILoginDisplayHost : public BaseLoginDisplayHost {
 public:
  explicit WebUILoginDisplayHost(const gfx::Rect& background_bounds);
  virtual ~WebUILoginDisplayHost();

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
  virtual void StartWizard(const std::string& first_screen_name,
                           const GURL& start_url) OVERRIDE;
  virtual void StartSignInScreen() OVERRIDE;

  // BaseLoginDisplayHost overrides:
  virtual WizardController* CreateWizardController() OVERRIDE;

 private:
  // Loads given URL. Creates WebUILoginView if needed.
  void LoadURL(const GURL& url);

  OobeUI* GetOobeUI() const;

  // Container of the screen we are displaying.
  views::Widget* login_window_;

  // Container of the view we are displaying.
  WebUILoginView* login_view_;

  DISALLOW_COPY_AND_ASSIGN(WebUILoginDisplayHost);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_WEBUI_LOGIN_DISPLAY_HOST_H_
