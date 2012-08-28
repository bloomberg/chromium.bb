// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/caps_lock_delegate_stub.h"

namespace ash {

CapsLockDelegateStub::CapsLockDelegateStub()
    : enabled_(false) {}

CapsLockDelegateStub::~CapsLockDelegateStub() {}

bool CapsLockDelegateStub::IsCapsLockEnabled() const {
  return enabled_;
}

void CapsLockDelegateStub::SetCapsLockEnabled(bool enabled) {
  enabled_ = enabled;
}

void CapsLockDelegateStub::ToggleCapsLock() {
  enabled_ = !enabled_;
}

}  // namespace ash
