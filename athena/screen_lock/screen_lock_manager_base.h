// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_SCREEN_LOCK_SCREEN_LOCK_MANAGER_BASE_H_
#define ATHENA_SCREEN_LOCK_SCREEN_LOCK_MANAGER_BASE_H_

#include "athena/screen_lock/public/screen_lock_manager.h"
#include "base/macros.h"

namespace athena {

// Base class for ScreenLockManager implementations. Its c-tors remembers
// instance pointer in global static variable for ScreenLockManager::Get
// implementation and d-tor clears that global variable.
class ScreenLockManagerBase : public ScreenLockManager {
 public:
  ScreenLockManagerBase();

 protected:
  ~ScreenLockManagerBase() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenLockManagerBase);
};

}  // namespace athena

#endif  // ATHENA_SCREEN_LOCK_SCREEN_LOCK_MANAGER_BASE_H_
