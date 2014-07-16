// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SESSION_STUB_LOGIN_SESSION_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SESSION_STUB_LOGIN_SESSION_MANAGER_DELEGATE_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/login/session/restore_after_crash_session_manager_delegate.h"

class Profile;

namespace chromeos {

class StubLoginSessionManagerDelegate
    : public RestoreAfterCrashSessionManagerDelegate {
 public:
  StubLoginSessionManagerDelegate(Profile* profile,
                                  const std::string& login_user_id);
  virtual ~StubLoginSessionManagerDelegate();

 private:
  // session_manager::SessionManagerDelegate implementation:
  virtual void Start() OVERRIDE;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(StubLoginSessionManagerDelegate);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SESSION_STUB_LOGIN_SESSION_MANAGER_DELEGATE_H_
