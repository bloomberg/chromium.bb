// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREEN_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREEN_OBSERVER_H_

#include <string>

namespace chromeos {

// Interface that handles notifications received from any of login wizard
// screens.
class ScreenObserver {
 public:
  // Each login screen or a view shown within login wizard view is itself a
  // state. Upon exit each view returns one of the results by calling
  // OnExit() method. Depending on the result and the current view or state
  // login wizard decides what is the next view to show. There must be an
  // exit code for each way to exit the screen for each screen.
  enum ExitCodes {
    // "Continue" was pressed on network screen and network is online.
    NETWORK_CONNECTED,
    // Connection failed while trying to load a WebPageScreen.
    CONNECTION_FAILED,
    UPDATE_INSTALLED,
    UPDATE_NOUPDATE,
    UPDATE_ERROR_CHECKING_FOR_UPDATE,
    UPDATE_ERROR_UPDATING,
    USER_IMAGE_SELECTED,
    EULA_ACCEPTED,
    EULA_BACK,
    ENTERPRISE_ENROLLMENT_COMPLETED,
    ENTERPRISE_AUTO_MAGIC_ENROLLMENT_COMPLETED,
    RESET_CANCELED,
    TERMS_OF_SERVICE_DECLINED,
    TERMS_OF_SERVICE_ACCEPTED,
    WRONG_HWID_WARNING_SKIPPED,
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

 protected:
  virtual ~ScreenObserver() {}
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREEN_OBSERVER_H_
