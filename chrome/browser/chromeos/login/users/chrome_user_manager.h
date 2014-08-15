// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USERS_CHROME_USER_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USERS_CHROME_USER_MANAGER_H_

#include "base/basictypes.h"
#include "components/user_manager/user_manager_base.h"

namespace base {
class TaskRunner;
}

namespace chromeos {

class MultiProfileUserController;
class SupervisedUserManager;
class UserFlow;
class UserImageManager;

// Chrome specific interface of the UserManager.
class ChromeUserManager : public user_manager::UserManagerBase {
 public:
  ChromeUserManager(scoped_refptr<base::TaskRunner> task_runner,
                    scoped_refptr<base::TaskRunner> blocking_task_runner);
  virtual ~ChromeUserManager();

  // Returns current ChromeUserManager or NULL if instance hasn't been
  // yet initialized.
  static ChromeUserManager* Get();

  virtual MultiProfileUserController* GetMultiProfileUserController() = 0;
  virtual UserImageManager* GetUserImageManager(const std::string& user_id) = 0;
  virtual SupervisedUserManager* GetSupervisedUserManager() = 0;

  // Method that allows to set |flow| for user identified by |user_id|.
  // Flow should be set before login attempt.
  // Takes ownership of the |flow|, |flow| will be deleted in case of login
  // failure.
  virtual void SetUserFlow(const std::string& user_id, UserFlow* flow) = 0;

  // Return user flow for current user. Returns instance of DefaultUserFlow if
  // no flow was defined for current user, or user is not logged in.
  // Returned value should not be cached.
  virtual UserFlow* GetCurrentUserFlow() const = 0;

  // Return user flow for user identified by |user_id|. Returns instance of
  // DefaultUserFlow if no flow was defined for user.
  // Returned value should not be cached.
  virtual UserFlow* GetUserFlow(const std::string& user_id) const = 0;

  // Resets user flow for user identified by |user_id|.
  virtual void ResetUserFlow(const std::string& user_id) = 0;

  DISALLOW_COPY_AND_ASSIGN(ChromeUserManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USERS_CHROME_USER_MANAGER_H_
