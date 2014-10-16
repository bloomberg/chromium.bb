// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_SCREEN_LOCK_PUBLIC_SCREEN_LOCK_MANAGER_H_
#define ATHENA_SCREEN_LOCK_PUBLIC_SCREEN_LOCK_MANAGER_H_

namespace athena {

// Manages screen lock and register keyboard shortcuts.
class ScreenLockManager {
 public:
  // Creates, returns and deletes the singleton object of the ScreenLockManager
  // implementation.
  static ScreenLockManager* Create();
  static ScreenLockManager* Get();
  static void Shutdown();

  virtual void LockScreen() = 0;

 protected:
  // Make d-tor protected to prevent destruction without calling Shutdown.
  virtual ~ScreenLockManager() {}
};

}  // namespace athena

#endif  // ATHENA_SCREEN_LOCK_PUBLIC_SCREEN_LOCK_MANAGER_H_
