// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_RECOVERY_UTIL_MAC_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_RECOVERY_UTIL_MAC_H_

#include "base/memory/scoped_refptr.h"

class PrefService;

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace password_manager {

// A utility class which is used by login database to store the date when some
// undecryptable logins were deleted.
class PasswordRecoveryUtilMac {
 public:
  PasswordRecoveryUtilMac(
      PrefService* local_state,
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner);

  ~PasswordRecoveryUtilMac();

  // Posts tasks on main thread to store the current time.
  void RecordPasswordRecovery();

 private:
  PrefService* local_state_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_RECOVERY_UTIL_MAC_H_
