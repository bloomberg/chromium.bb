// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_CONTROLLER_H_

#include <string>

#include "base/scoped_ptr.h"
#include "chrome/browser/chromeos/login/screen_observer.h"
#include "chrome/browser/chromeos/login/view_screen.h"
#include "chrome/browser/chromeos/login/wizard_screen.h"
#include "chrome/browser/chromeos/status/status_area_host.h"
#include "views/window/window_delegate.h"

class AccountScreen;
class WizardContentsView;
class WizardScreen;
namespace chromeos {
class StatusAreaView;
}  // namespace chromeos

// Class that manages control flow between wizard screens, top level window,
// status area and background view. Wizard controller interacts with screen
// controllers to move the user between screens.
class WizardController : public views::WindowDelegate,
                         public chromeos::StatusAreaHost,
                         public chromeos::ScreenObserver,
                         public WizardScreenDelegate {
 public:
  WizardController();
  virtual ~WizardController();

  // Shows the first screen defined by |first_screen_name| or by default
  // if the parameter is empty.
  void ShowFirstScreen(const std::string& first_screen_name);

 private:
  // Exit handlers:
  void OnLoginSignInSelected();
  void OnLoginCreateAccount();
  void OnNetworkConnected();
  void OnAccountCreated();

  // Overridden from chromeos::ScreenObserver:
  virtual void OnExit(ExitCodes exit_code);

  // Overridden from views::WindowDelegate:
  virtual views::View* GetContentsView();

  // Overridden from StatusAreaHost:
  virtual gfx::NativeWindow GetNativeWindow() const;
  virtual bool ShouldOpenButtonOptions(const views::View* button_view) const;
  virtual void OpenButtonOptions(const views::View* button_view) const;
  virtual bool IsButtonVisible(const views::View* button_view) const;

  // Overridden from WizardScreenDelegate:
  virtual views::View* GetWizardView();
  virtual views::Window* GetWizardWindow();
  virtual chromeos::ScreenObserver* GetObserver(WizardScreen* screen);

  // Initializes contents view and status area.
  void InitContents();

  // Lazy initializers and getters for screens.
  NetworkScreen* GetNetworkScreen();
  LoginScreen* GetLoginScreen();
  AccountScreen* GetAccountScreen();
  UpdateScreen* GetUpdateScreen();

  // Switches from one screen to another.
  void SetCurrentScreen(WizardScreen* screen);

  // Contents view.
  WizardContentsView* contents_;

  // Status area view.
  chromeos::StatusAreaView* status_area_;

  // Screens.
  scoped_ptr<NetworkScreen> network_screen_;
  scoped_ptr<LoginScreen> login_screen_;
  scoped_ptr<AccountScreen> account_screen_;
  scoped_ptr<UpdateScreen> update_screen_;

  // Screen that's currently active.
  WizardScreen* current_screen_;

  DISALLOW_COPY_AND_ASSIGN(WizardController);
};

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_CONTROLLER_H_
