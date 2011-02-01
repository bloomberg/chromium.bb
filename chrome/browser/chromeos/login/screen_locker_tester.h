// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREEN_LOCKER_TESTER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREEN_LOCKER_TESTER_H_
#pragma once

#include <string>

#include "base/basictypes.h"

namespace views {
class Button;
class Textfield;
class Widget;
}  // namespace views

namespace chromeos {

class ScreenLocker;

namespace test {

// ScreenLockerTester provides access to the private state/function
// of ScreenLocker class. Used to implement unit tests.
class ScreenLockerTester {
 public:
  // Returns true if the screen is locked.
  bool IsLocked();

  // Injects MockAuthenticate that uses given |user| and |password|.
  void InjectMockAuthenticator(const std::string& user,
                               const std::string& password);

  // Sets the password text.
  void SetPassword(const std::string& password);

  // Gets the password text.
  std::string GetPassword() const;

  // Emulates entring a password.
  void EnterPassword(const std::string& password);

  // Emulates the ready message from window manager.
  void EmulateWindowManagerReady();

  // Returns the widget for screen locker window.
  views::Widget* GetWidget() const;

  views::Widget* GetChildWidget() const;

 private:
  friend class chromeos::ScreenLocker;

  ScreenLockerTester() {}

  views::Textfield* GetPasswordField() const;

  DISALLOW_COPY_AND_ASSIGN(ScreenLockerTester);
};

}  // namespace test

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREEN_LOCKER_TESTER_H_
