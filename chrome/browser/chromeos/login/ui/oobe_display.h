// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UI_OOBE_DISPLAY_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UI_OOBE_DISPLAY_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"

namespace chromeos {

class AppLaunchSplashScreenActor;
class AutoEnrollmentCheckScreenActor;
class CoreOobeActor;
class ControllerPairingScreenActor;
class EnrollmentScreenActor;
class ErrorScreenActor;
class EulaScreenActor;
class HIDDetectionScreenActor;
class HostPairingScreenActor;
class KioskAutolaunchScreenActor;
class KioskEnableScreenActor;
class NetworkScreenActor;
class ResetScreenActor;
class SupervisedUserCreationScreenHandler;
class TermsOfServiceScreenActor;
class UpdateScreenActor;
class UserImageScreenActor;
class WrongHWIDScreenActor;

// Interface which is used by WizardController to do actual OOBE screens
// showing. Also it provides actors for the OOBE screens.
class OobeDisplay {
 public:
  enum Screen {
    SCREEN_OOBE_HID_DETECTION = 0,
    SCREEN_OOBE_NETWORK,
    SCREEN_OOBE_EULA,
    SCREEN_OOBE_UPDATE,
    SCREEN_OOBE_ENROLLMENT,
    SCREEN_OOBE_RESET,
    SCREEN_GAIA_SIGNIN,
    SCREEN_ACCOUNT_PICKER,
    SCREEN_KIOSK_AUTOLAUNCH,
    SCREEN_KIOSK_ENABLE,
    SCREEN_ERROR_MESSAGE,
    SCREEN_USER_IMAGE_PICKER,
    SCREEN_TPM_ERROR,
    SCREEN_PASSWORD_CHANGED,
    SCREEN_CREATE_SUPERVISED_USER_DIALOG,
    SCREEN_CREATE_SUPERVISED_USER_FLOW,
    SCREEN_TERMS_OF_SERVICE,
    SCREEN_WRONG_HWID,
    SCREEN_AUTO_ENROLLMENT_CHECK,
    SCREEN_APP_LAUNCH_SPLASH,
    SCREEN_CONFIRM_PASSWORD,
    SCREEN_FATAL_ERROR,
    SCREEN_OOBE_CONTROLLER_PAIRING,
    SCREEN_OOBE_HOST_PAIRING,
    SCREEN_UNKNOWN
  };

  virtual ~OobeDisplay() {}

  // Pointers to actors which should be used by the specific screens. Actors
  // must be owned by the OobeDisplay implementation.
  virtual CoreOobeActor* GetCoreOobeActor() = 0;
  virtual UpdateScreenActor* GetUpdateScreenActor() = 0;
  virtual NetworkScreenActor* GetNetworkScreenActor() = 0;
  virtual EulaScreenActor* GetEulaScreenActor() = 0;
  virtual EnrollmentScreenActor* GetEnrollmentScreenActor() = 0;
  virtual ResetScreenActor* GetResetScreenActor() = 0;
  virtual KioskAutolaunchScreenActor* GetKioskAutolaunchScreenActor() = 0;
  virtual KioskEnableScreenActor* GetKioskEnableScreenActor() = 0;
  virtual TermsOfServiceScreenActor* GetTermsOfServiceScreenActor() = 0;
  virtual UserImageScreenActor* GetUserImageScreenActor() = 0;
  virtual ErrorScreenActor* GetErrorScreenActor() = 0;
  virtual WrongHWIDScreenActor* GetWrongHWIDScreenActor() = 0;
  virtual AutoEnrollmentCheckScreenActor*
      GetAutoEnrollmentCheckScreenActor() = 0;
  virtual HIDDetectionScreenActor* GetHIDDetectionScreenActor() = 0;
  virtual SupervisedUserCreationScreenHandler*
      GetSupervisedUserCreationScreenActor() = 0;
  virtual AppLaunchSplashScreenActor* GetAppLaunchSplashScreenActor() = 0;
  virtual ControllerPairingScreenActor* GetControllerPairingScreenActor() = 0;
  virtual HostPairingScreenActor* GetHostPairingScreenActor() = 0;

  // Returns if JS side is fully loaded and ready to accept messages.
  // If |false| is returned, then |display_is_ready_callback| is stored
  // and will be called once display is ready.
  virtual bool IsJSReady(const base::Closure& display_is_ready_callback) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UI_OOBE_DISPLAY_H_
