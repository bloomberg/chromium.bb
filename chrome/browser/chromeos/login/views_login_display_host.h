// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_VIEWS_LOGIN_DISPLAY_HOST_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_VIEWS_LOGIN_DISPLAY_HOST_H_
#pragma once

#include <string>

#include "chrome/browser/chromeos/login/background_view.h"
#include "chrome/browser/chromeos/login/base_login_display_host.h"
#include "ui/gfx/rect.h"

namespace chromeos {

class ViewsOobeDisplay;

// Views-specific implementation of the OOBE/login screen host.
// Uses ViewsLoginDisplay as the login screen UI implementation,
// BackgroundView as the background UI implementation.
// In its current implementation BackgroundView encapsulates StatusAreaView.
class ViewsLoginDisplayHost : public chromeos::BaseLoginDisplayHost {
 public:
  explicit ViewsLoginDisplayHost(const gfx::Rect& background_bounds);
  virtual ~ViewsLoginDisplayHost();

  // LoginDisplayHost implementation:
  virtual LoginDisplay* CreateLoginDisplay(LoginDisplay::Delegate* delegate);
  virtual gfx::NativeWindow GetNativeWindow() const;
  virtual void SetOobeProgress(BackgroundView::LoginStep step);
  virtual void SetOobeProgressBarVisible(bool visible);
  virtual void SetShutdownButtonEnabled(bool enable);
  virtual void SetStatusAreaEnabled(bool enable);
  virtual void SetStatusAreaVisible(bool visible);
  virtual void ShowBackground();
  virtual void StartSignInScreen();

  // BaseLoginDisplayHost implementation:
  virtual WizardController* CreateWizardController() OVERRIDE;

 private:
  // Background view/window.
  BackgroundView* background_view_;
  views::Widget* background_window_;

  // Keeps views based OobeDisplay implementation.
  scoped_ptr<ViewsOobeDisplay> oobe_display_;

  DISALLOW_COPY_AND_ASSIGN(ViewsLoginDisplayHost);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_VIEWS_LOGIN_DISPLAY_HOST_H_
