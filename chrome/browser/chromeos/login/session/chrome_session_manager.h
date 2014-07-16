// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SESSION_CHROME_SESSION_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SESSION_CHROME_SESSION_MANAGER_H_

#include "base/basictypes.h"
#include "components/session_manager/core/session_manager.h"

namespace base {
class CommandLine;
}

class Profile;

namespace chromeos {

class ChromeSessionManager : public session_manager::SessionManager {
 public:
  static scoped_ptr<session_manager::SessionManager> CreateSessionManager(
      const base::CommandLine& parsed_command_line,
      Profile* profile,
      bool is_running_test);

 private:
  explicit ChromeSessionManager(
      session_manager::SessionManagerDelegate* delegate);
  virtual ~ChromeSessionManager();

  DISALLOW_COPY_AND_ASSIGN(ChromeSessionManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SESSION_CHROME_SESSION_MANAGER_H_
