// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/gfx/canvas.h"
#include "chrome/browser/chromeos/login_manager_view.h"
#include "views/view.h"
#include "views/window/window_delegate.h"

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_VIEW_H_

// View for the wizard that will launch OOBE steps or login screen.
class LoginWizardView : public views::View,
                        public views::WindowDelegate,
                        public LoginManagerView::LoginObserver {
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

 private:
   // Creates login window.
   void InitLoginWindow();

  // Creates main wizard window with status bar.
  void InitWizardWindow();

  // Background for the wizard view.
  GdkPixbuf* background_pixbuf_;

  // Wizard view dimensions.
  gfx::Size dimensions_;

  DISALLOW_COPY_AND_ASSIGN(LoginWizardView);
};

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_VIEW_H_
