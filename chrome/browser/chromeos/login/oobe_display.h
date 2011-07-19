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
class UserImageScreenActor;
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

  // Pointers to actors which should be used by the specific screens. Actors
  // must be owned by the OobeDisplay implementaion.
  virtual UpdateScreenActor* GetUpdateScreenActor() = 0;
  virtual NetworkScreenActor* GetNetworkScreenActor() = 0;
  virtual EulaScreenActor* GetEulaScreenActor() = 0;
  virtual UserImageScreenActor* GetUserImageScreenActor() = 0;
  // TODO(altimofeev): use real actors instead.
  virtual ViewScreenDelegate* GetEnterpriseEnrollmentScreenActor() = 0;
  virtual ViewScreenDelegate* GetRegistrationScreenActor() = 0;
  virtual ViewScreenDelegate* GetHTMLPageScreenActor() = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_OOBE_DISPLAY_H_
