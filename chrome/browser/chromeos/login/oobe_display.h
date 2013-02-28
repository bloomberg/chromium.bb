// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_OOBE_DISPLAY_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_OOBE_DISPLAY_H_

#include <string>

#include "base/basictypes.h"

namespace chromeos {

class EnterpriseEnrollmentScreenActor;
class EulaScreenActor;
class NetworkScreenActor;
class ResetScreenActor;
class TermsOfServiceScreenActor;
class UpdateScreenActor;
class UserImageScreenActor;
// TODO(altimofeev): use real actors instead
class ViewScreenDelegate;
class WizardScreen;
class WrongHWIDScreenActor;
class LocallyManagedUserCreationScreenHandler;

// Interface which is used by WizardController to do actual OOBE screens
// showing. Also it provides actors for the OOBE screens.
class OobeDisplay {
 public:
  virtual ~OobeDisplay() {}

  // Shows the given screen.
  virtual void ShowScreen(WizardScreen* screen) = 0;

  // Hides the given screen.
  virtual void HideScreen(WizardScreen* screen) = 0;

  // Pointers to actors which should be used by the specific screens. Actors
  // must be owned by the OobeDisplay implementation.
  virtual UpdateScreenActor* GetUpdateScreenActor() = 0;
  virtual NetworkScreenActor* GetNetworkScreenActor() = 0;
  virtual EulaScreenActor* GetEulaScreenActor() = 0;
  virtual EnterpriseEnrollmentScreenActor*
      GetEnterpriseEnrollmentScreenActor() = 0;
  virtual ResetScreenActor* GetResetScreenActor() = 0;
  virtual TermsOfServiceScreenActor* GetTermsOfServiceScreenActor() = 0;
  virtual UserImageScreenActor* GetUserImageScreenActor() = 0;
  // TODO(altimofeev): use real actors instead.
  virtual ViewScreenDelegate* GetRegistrationScreenActor() = 0;
  virtual WrongHWIDScreenActor* GetWrongHWIDScreenActor() = 0;
  virtual LocallyManagedUserCreationScreenHandler*
      GetLocallyManagedUserCreationScreenActor() = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_OOBE_DISPLAY_H_
