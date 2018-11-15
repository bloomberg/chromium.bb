// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LOCK_SCREEN_LOCKER_TESTER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LOCK_SCREEN_LOCKER_TESTER_H_

#include <memory>
#include <string>

namespace chromeos {

class UserContext;

// ScreenLockerTester provides access to the private state/function
// of ScreenLocker class. Used to implement unit tests.
class ScreenLockerTester {
 public:
  // Create a new tester.
  static std::unique_ptr<ScreenLockerTester> Create();

  ScreenLockerTester();
  virtual ~ScreenLockerTester();

  // Returns true if the screen is locked.
  virtual bool IsLocked();

  // Injects StubAuthenticator that uses the credentials in |user_context|.
  virtual void InjectStubUserContext(const UserContext& user_context);

  // Enters and submits the given password.
  virtual void EnterPassword(const std::string& password) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_LOCK_SCREEN_LOCKER_TESTER_H_
