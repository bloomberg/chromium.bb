// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
    LOGIN_SIGN_IN_SELECTED,
    LOGIN_BACK,
    LOGIN_CREATE_ACCOUNT,
    NETWORK_CONNECTED,
    NETWORK_OFFLINE,
    ACCOUNT_CREATED,
    LANGUAGE_CHANGED,
    UPDATE_INSTALLED,
    UPDATE_NOUPDATE,
    UPDATE_NETWORK_ERROR,
    UPDATE_OTHER_ERROR,
  };

  // Method called by a screen when user's done with it.
  virtual void OnExit(ExitCodes exit_code) = 0;

  // Switch to the new language. |lang| specifies new language locale code.
  // Caution: this callback resets (deletes and re-creates) all views
  // (including *this), so do not access it after you call this!
  virtual void OnSwitchLanguage(const std::string& lang) = 0;

 protected:
  virtual ~ScreenObserver() {}
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREEN_OBSERVER_H_
