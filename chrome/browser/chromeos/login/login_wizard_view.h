// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_WIZARD_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_WIZARD_VIEW_H_

#include "app/gfx/canvas.h"
#include "chrome/browser/chromeos/login/login_manager_view.h"
#include "chrome/browser/chromeos/status/status_area_host.h"
#include "views/view.h"
#include "views/window/window_delegate.h"

namespace chromeos {
class StatusAreaView;
}  // namespace chromeos

// View for the wizard that will launch OOBE steps or login screen.
class LoginWizardView : public views::View,
                        public views::WindowDelegate,
                        public LoginManagerView::LoginObserver,
                        public chromeos::StatusAreaHost {
 public:
  LoginWizardView();
  virtual ~LoginWizardView();

  // Initializes wizard windows and controls (status bar).
  void Init();

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize();

  // Overridden from views::WindowDelegate:
  virtual views::View* GetContentsView();

  // LoginObserver notification.
  virtual void OnLogin();

  // Overriden from StatusAreaHost:
  virtual gfx::NativeWindow GetNativeWindow() const;
  virtual bool ShouldOpenButtonOptions(const views::View* button_view) const;
  virtual void OpenButtonOptions(const views::View* button_view) const;
  virtual bool IsButtonVisible(const views::View* button_view) const;

 private:
  // Creates login window.
  void InitLoginWindow();

  // Creates main wizard window with status bar.
  void InitWizardWindow();

  // Background for the wizard view.
  GdkPixbuf* background_pixbuf_;

  // Wizard view dimensions.
  gfx::Size dimensions_;

  // Status area view.
  chromeos::StatusAreaView* status_area_;

  DISALLOW_COPY_AND_ASSIGN(LoginWizardView);
};

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_WIZARD_VIEW_H_
