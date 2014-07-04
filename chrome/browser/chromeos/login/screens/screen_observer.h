// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_SCREEN_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_SCREEN_OBSERVER_H_

#include <string>

namespace chromeos {

class ErrorScreen;
class WizardScreen;

// Interface that handles notifications received from any of login wizard
// screens.
class ScreenObserver {
 public:
  // Each login screen or a view shown within login wizard view is itself a
  // state. Upon exit each view returns one of the results by calling OnExit()
  // method. Depending on the result and the current view or state login wizard
  // decides what is the next view to show. There must be an exit code for each
  // way to exit the screen for each screen. (Numeric ids are provided to
  // facilitate interpretation of log files only, they are subject to change
  // without notice.)
  enum ExitCodes {
    // "Continue" was pressed on network screen and network is online.
    NETWORK_CONNECTED = 0,
    HID_DETECTION_COMPLETED = 1,
    // Connection failed while trying to load a WebPageScreen.
    CONNECTION_FAILED = 2,
    UPDATE_INSTALLED = 3,
    UPDATE_NOUPDATE = 4,
    UPDATE_ERROR_CHECKING_FOR_UPDATE = 5,
    UPDATE_ERROR_UPDATING = 6,
    USER_IMAGE_SELECTED = 7,
    EULA_ACCEPTED = 8,
    EULA_BACK = 9,
    ENTERPRISE_AUTO_ENROLLMENT_CHECK_COMPLETED = 10,
    ENTERPRISE_ENROLLMENT_COMPLETED = 11,
    ENTERPRISE_AUTO_MAGIC_ENROLLMENT_COMPLETED = 12,
    ENTERPRISE_ENROLLMENT_BACK = 13,
    RESET_CANCELED = 14,
    KIOSK_AUTOLAUNCH_CANCELED = 15,
    KIOSK_AUTOLAUNCH_CONFIRMED = 16,
    KIOSK_ENABLE_COMPLETED = 17,
    TERMS_OF_SERVICE_DECLINED = 18,
    TERMS_OF_SERVICE_ACCEPTED = 19,
    WRONG_HWID_WARNING_SKIPPED = 20,
    CONTROLLER_PAIRING_FINISHED = 21,
    HOST_PAIRING_FINISHED = 22,
    EXIT_CODES_COUNT  // not a real code, must be the last
  };

  // Method called by a screen when user's done with it.
  virtual void OnExit(ExitCodes exit_code) = 0;

  // Forces current screen showing.
  virtual void ShowCurrentScreen() = 0;

  // Notify about new user names and password. It is used to autologin
  // just created user without asking the same info once again.
  virtual void OnSetUserNamePassword(const std::string& username,
                                     const std::string& password) = 0;

  // Whether usage statistics reporting is enabled on EULA screen.
  virtual void SetUsageStatisticsReporting(bool val) = 0;
  virtual bool GetUsageStatisticsReporting() const = 0;

  virtual ErrorScreen* GetErrorScreen() = 0;
  virtual void ShowErrorScreen() = 0;
  virtual void HideErrorScreen(WizardScreen* parent_screen) = 0;

 protected:
  virtual ~ScreenObserver() {}
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_SCREEN_OBSERVER_H_
