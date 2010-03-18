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

class AccountScreen;
class WizardContentsView;
class WizardScreen;

namespace chromeos {
class BackgroundView;
}

namespace gfx {
class Rect;
}

namespace views {
class Views;
class Widget;
}

// Class that manages control flow between wizard screens. Wizard controller
// interacts with screen controllers to move the user between screens.
class WizardController : public chromeos::ScreenObserver,
                         public WizardScreenDelegate {
 public:
  WizardController();
  ~WizardController();

  // Returns the default wizard controller if it has been created.
  static WizardController* default_controller() {
    return default_controller_;
  }

  // Shows the first screen defined by |first_screen_name| or by default
  // if the parameter is empty. |paint_background| indicates whether a
  // background should be painted. If |paint_background| is false, the window is
  // made transparent. |screen_bounds| are used to calculate position of the
  // wizard screen.
  void Init(const std::string& first_screen_name,
            const gfx::Rect& screen_bounds,
            bool paint_background);

  // Returns the view that contains all the other views.
  views::View* contents() { return contents_; }

  // Shows the wizard controller in a window.
  void Show();

  // Creates and shows a background window.
  void ShowBackground(const gfx::Rect& bounds);

  // Takes ownership of the specified background widget and view.
  void OwnBackground(views::Widget* background_widget,
                     chromeos::BackgroundView* background_view);

  // Lazy initializers and getters for screens.
  NetworkScreen* GetNetworkScreen();
  LoginScreen* GetLoginScreen();
  AccountScreen* GetAccountScreen();
  UpdateScreen* GetUpdateScreen();

 private:
  // Exit handlers:
  void OnLoginSignInSelected();
  void OnLoginCreateAccount();
  void OnNetworkConnected();
  void OnAccountCreated();
  void OnLanguageChanged();
  void OnUpdateCompleted();
  void OnUpdateNetworkError();

  // Overridden from chromeos::ScreenObserver:
  virtual void OnExit(ExitCodes exit_code);
  virtual void OnSwitchLanguage(const std::string& lang);

  // Overridden from WizardScreenDelegate:
  virtual views::View* GetWizardView();
  virtual chromeos::ScreenObserver* GetObserver(WizardScreen* screen);

  // Switches from one screen to another.
  void SetCurrentScreen(WizardScreen* screen);

  void ShowFirstScreen(const std::string& first_screen_name);

  // Widget we're showing in.
  views::Widget* widget_;

  // Used to render the background.
  views::Widget* background_widget_;
  chromeos::BackgroundView* background_view_;

  // Contents view.
  views::View* contents_;

  // Screens.
  scoped_ptr<NetworkScreen> network_screen_;
  scoped_ptr<LoginScreen> login_screen_;
  scoped_ptr<AccountScreen> account_screen_;
  scoped_ptr<UpdateScreen> update_screen_;

  // Screen that's currently active.
  WizardScreen* current_screen_;

  // True if full OOBE flow should be shown.
  bool is_out_of_box_;

  // Default WizardController.
  static WizardController* default_controller_;

  DISALLOW_COPY_AND_ASSIGN(WizardController);
};

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_CONTROLLER_H_
