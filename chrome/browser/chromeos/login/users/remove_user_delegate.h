// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USERS_REMOVE_USER_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USERS_REMOVE_USER_DELEGATE_H_

namespace chromeos {

// Delegate to be used with |UserManager::RemoveUser|.
class RemoveUserDelegate {
 public:
  // Called right before actual user removal process is initiated.
  virtual void OnBeforeUserRemoved(const std::string& username) = 0;

  // Called right after user removal process has been initiated.
  virtual void OnUserRemoved(const std::string& username) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USERS_REMOVE_USER_DELEGATE_H_
