// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MANAGED_LOCALLY_MANAGED_USER_CREATION_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MANAGED_LOCALLY_MANAGED_USER_CREATION_SCREEN_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/login/wizard_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/locally_managed_user_creation_screen_handler.h"

namespace chromeos {

class LocallyManagedUserController;

// Class that controls screen showing ui for locally managed user creation.

class LocallyManagedUserCreationScreen : public WizardScreen,
    public LocallyManagedUserCreationScreenHandler::Delegate {
 public:
  LocallyManagedUserCreationScreen(ScreenObserver* observer,
      LocallyManagedUserCreationScreenHandler* actor);
  virtual ~LocallyManagedUserCreationScreen();

  virtual void SetParameters(string16 name, std::string password);

  // WizardScreen implementation:
  virtual void PrepareToShow() OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual std::string GetName() const OVERRIDE;

  // WrongHWIDScreenActor::Delegate implementation:
  virtual void OnExit() OVERRIDE;
  virtual void OnActorDestroyed(LocallyManagedUserCreationScreenHandler* actor)
      OVERRIDE;
  virtual void AbortFlow() OVERRIDE;
  virtual void FinishFlow() OVERRIDE;

 private:
  LocallyManagedUserCreationScreenHandler* actor_;

  scoped_ptr<LocallyManagedUserController> controller_;

  string16 name_;

  std::string password_;

  DISALLOW_COPY_AND_ASSIGN(LocallyManagedUserCreationScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MANAGED_LOCALLY_MANAGED_USER_CREATION_SCREEN_H_

