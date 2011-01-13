// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREEN_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREEN_OBSERVER_H_
#pragma once

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
    NETWORK_CONNECTED,
    NETWORK_OFFLINE,
    ACCOUNT_CREATE_BACK,
    ACCOUNT_CREATED,
    CONNECTION_FAILED,
    UPDATE_INSTALLED,
    UPDATE_NOUPDATE,
    UPDATE_ERROR_CHECKING_FOR_UPDATE,
    UPDATE_ERROR_UPDATING,
    USER_IMAGE_SELECTED,
    USER_IMAGE_SKIPPED,
    EULA_ACCEPTED,
    EULA_BACK,
    REGISTRATION_SUCCESS,
    REGISTRATION_SKIPPED,
    EXIT_CODES_COUNT  // not a real code, must be the last
  };

  // Method called by a screen when user's done with it.
  virtual void OnExit(ExitCodes exit_code) = 0;

  // Notify about new user names and password. It is used to autologin
  // just created user without asking the same info once again.
  virtual void OnSetUserNamePassword(const std::string& username,
                                     const std::string& password) = 0;

 protected:
  virtual ~ScreenObserver() {}
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREEN_OBSERVER_H_
