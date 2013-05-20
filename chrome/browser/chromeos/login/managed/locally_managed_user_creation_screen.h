// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MANAGED_LOCALLY_MANAGED_USER_CREATION_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MANAGED_LOCALLY_MANAGED_USER_CREATION_SCREEN_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/login/managed/locally_managed_user_controller.h"
#include "chrome/browser/chromeos/login/screens/wizard_screen.h"
#include "chrome/browser/chromeos/net/network_portal_detector.h"
#include "chrome/browser/ui/webui/chromeos/login/locally_managed_user_creation_screen_handler.h"

namespace chromeos {

// Class that controls screen showing ui for locally managed user creation.
class LocallyManagedUserCreationScreen
    : public WizardScreen,
      public LocallyManagedUserCreationScreenHandler::Delegate,
      public LocallyManagedUserController::StatusConsumer,
      public NetworkPortalDetector::Observer {
 public:
  LocallyManagedUserCreationScreen(
      ScreenObserver* observer,
      LocallyManagedUserCreationScreenHandler* actor);
  virtual ~LocallyManagedUserCreationScreen();

  // Makes screen to show message about inconsistency in manager login flow
  // (e.g. password change detected, invalid OAuth token, etc).
  // Called when manager user is successfully authenticated, so ui elements
  // should result in forced logout.
  void ShowManagerInconsistentStateErrorScreen();

  // Called when authentication fails for manager with provided password.
  // Displays wrong password message on manager selection screen.
  void OnManagerLoginFailure();

  // Called when manager is successfully authenticated and account is in
  // consistent state.
  void OnManagerFullyAuthenticated();

  // Called when manager is successfully authenticated against cryptohome, but
  // OAUTH token validation hasn't completed yet.
  // Results in spinner indicating that creation is in process.
  void OnManagerCryptohomeAuthenticated();

  // Shows initial screen where managed user name/password are defined and
  // manager is selected.
  void ShowInitialScreen();

  // WizardScreen implementation:
  virtual void PrepareToShow() OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual std::string GetName() const OVERRIDE;

  // LocallyManagedUserCreationScreenHandler::Delegate implementation:
  virtual void OnExit() OVERRIDE;
  virtual void OnActorDestroyed(LocallyManagedUserCreationScreenHandler* actor)
      OVERRIDE;
  virtual void CreateManagedUser(
      const string16& display_name,
      const std::string& managed_user_password) OVERRIDE;
  virtual void AuthenticateManager(
      const std::string& manager_id,
      const std::string& manager_password) OVERRIDE;
  virtual void AbortFlow() OVERRIDE;
  virtual void FinishFlow() OVERRIDE;
  virtual void SelectPicture() OVERRIDE;

  // LocallyManagedUserController::StatusConsumer overrides.
  virtual void OnCreationError(LocallyManagedUserController::ErrorCode code,
                               bool recoverable) OVERRIDE;
  virtual void OnCreationSuccess() OVERRIDE;

  // ConnectivityStateHelperObserver implementation:
  virtual void OnPortalDetectionCompleted(
          const Network* network,
          const NetworkPortalDetector::CaptivePortalState& state) OVERRIDE;
 private:
  LocallyManagedUserCreationScreenHandler* actor_;

  scoped_ptr<LocallyManagedUserController> controller_;

  bool on_error_screen_;
  bool on_image_screen_;

  DISALLOW_COPY_AND_ASSIGN(LocallyManagedUserCreationScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MANAGED_LOCALLY_MANAGED_USER_CREATION_SCREEN_H_

