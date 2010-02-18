// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_WIZARD_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_WIZARD_VIEW_H_

#include <string>

#include "app/gfx/canvas.h"
#include "chrome/browser/chromeos/login/screen_observer.h"
#include "chrome/browser/chromeos/status/status_area_host.h"
#include "views/view.h"
#include "views/window/window_delegate.h"

class AccountCreationView;
class LoginManagerView;
class NetworkSelectionView;
class TestRendererScreen;
class WizardScreen;

namespace chromeos {
class StatusAreaView;
}  // namespace chromeos

// View for the wizard that will launch OOBE steps or login screen.
class LoginWizardView : public views::View,
                        public views::WindowDelegate,
                        public chromeos::StatusAreaHost,
                        public chromeos::ScreenObserver {
 public:
  LoginWizardView();
  virtual ~LoginWizardView() {}

  // Initializes wizard windows and controls (status bar).
  void Init(const std::string& start_view_name, const gfx::Size size);

 private:
  // Exit handlers:
  void OnLoginSignInSelected();
  void OnLoginBack();
  void OnNetworkConnected();
  void OnAccountCreated();

  // Overridden from chromeos::ScreenObserver:
  virtual void OnExit(ExitCodes exit_code);

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize();

  // Overridden from views::WindowDelegate:
  virtual views::View* GetContentsView();

  // Overridden from StatusAreaHost:
  virtual gfx::NativeWindow GetNativeWindow() const;
  virtual bool ShouldOpenButtonOptions(const views::View* button_view) const;
  virtual void OpenButtonOptions(const views::View* button_view) const;
  virtual bool IsButtonVisible(const views::View* button_view) const;

  // Initializer for status area.
  void InitStatusArea();

  // Initializer for all login screens.
  template <class T>
  void CreateAndInitScreen(T** screen);

  // Switches the current screen to another one.
  void SetCurrentScreen(WizardScreen* screen);

  // Wizard view dimensions.
  gfx::Size dimensions_;

  // Status area view.
  chromeos::StatusAreaView* status_area_;

  // View that's shown to the user at the moment.
  WizardScreen* current_;

  // Login manager view.
  LoginManagerView* login_manager_;

  // Network selection/welcome view.
  NetworkSelectionView* network_selection_;

  // Test Renderer Screen. To be used for testing.
  // TODO(davemoore) Remove this after done.
  TestRendererScreen* test_renderer_;

  // Account creation view.
  AccountCreationView* account_creation_;

  DISALLOW_COPY_AND_ASSIGN(LoginWizardView);
};

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_WIZARD_VIEW_H_
