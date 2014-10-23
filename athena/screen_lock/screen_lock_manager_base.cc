// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/screen_lock/screen_lock_manager_base.h"

#include "base/logging.h"

namespace athena {
namespace {

ScreenLockManager* instance = nullptr;

}  // namespace

ScreenLockManagerBase::ScreenLockManagerBase() {
  DCHECK(!instance);
  instance = this;
}

ScreenLockManagerBase::~ScreenLockManagerBase() {
  DCHECK_EQ(instance, this);
  instance = nullptr;
}

// static
ScreenLockManager* ScreenLockManager::Get() {
  return instance;
}

// static
void ScreenLockManager::Shutdown() {
  if (instance) {
    delete instance;
    DCHECK(!instance);
  }
}

}  // namespace athena
