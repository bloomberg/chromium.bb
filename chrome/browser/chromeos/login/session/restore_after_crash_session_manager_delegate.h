// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SESSION_RESTORE_AFTER_CRASH_SESSION_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SESSION_RESTORE_AFTER_CRASH_SESSION_MANAGER_DELEGATE_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "components/session_manager/core/session_manager.h"

class Profile;

namespace chromeos {

class RestoreAfterCrashSessionManagerDelegate
    : public session_manager::SessionManagerDelegate {
 public:
  RestoreAfterCrashSessionManagerDelegate(Profile* profile,
                                          const std::string& login_user_id);
  virtual ~RestoreAfterCrashSessionManagerDelegate();

 protected:
  // session_manager::SessionManagerDelegate implementation:
  virtual void Start() OVERRIDE;

  Profile* profile() { return profile_; }
  const std::string& login_user_id() const { return login_user_id_; }

 private:
  Profile* profile_;
  const std::string login_user_id_;

  DISALLOW_COPY_AND_ASSIGN(RestoreAfterCrashSessionManagerDelegate);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SESSION_RESTORE_AFTER_CRASH_SESSION_MANAGER_DELEGATE_H_
