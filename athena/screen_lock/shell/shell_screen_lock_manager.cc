// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/screen_lock/screen_lock_manager_base.h"
#include "base/macros.h"

namespace athena {

ScreenLockManager* ScreenLockManager::Create() {
  return nullptr;
}

}  // namespace athena