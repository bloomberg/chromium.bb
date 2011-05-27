// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_OOBE_DISPLAY_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_OOBE_DISPLAY_H_
#pragma once

#include <string>
#include <base/basictypes.h>

namespace chromeos {

class EulaScreenActor;
class ScreenObserver;
class UpdateScreenActor;
class NetworkScreenActor;
// TODO(altimofeev): use real actors instead
class ViewScreenDelegate;
class WizardScreen;

// Interface which is used by WizardController to do actual OOBE screens
// showing. Also it provides actors for the OOBE screens.
class OobeDisplay {
 public:
  virtual ~OobeDisplay() {}

  // Shows the given screen.
  virtual void ShowScreen(WizardScreen* screen) = 0;

  // Hides the given screen.
  virtual void HideScreen(WizardScreen* screen) = 0;

  // Actors to be used by the specific screens.
  virtual UpdateScreenActor* CreateUpdateScreenActor() = 0;
  virtual NetworkScreenActor* CreateNetworkScreenActor() = 0;
  virtual EulaScreenActor* CreateEulaScreenActor() = 0;
  // TODO: use real actors instead.
  virtual ViewScreenDelegate* CreateEnterpriseEnrollmentScreenActor() = 0;
  virtual ViewScreenDelegate* CreateUserImageScreenActor() = 0;
  virtual ViewScreenDelegate* CreateRegistrationScreenActor() = 0;
  virtual ViewScreenDelegate* CreateHTMLPageScreenActor() = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_OOBE_DISPLAY_H_
