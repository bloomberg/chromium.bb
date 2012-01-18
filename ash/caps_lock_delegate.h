// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPS_LOCK_DELEGATE_H_
#define ASH_CAPS_LOCK_DELEGATE_H_
#pragma once

namespace ash {

// Delegate for toggling Caps Lock.
class CapsLockDelegate {
 public:
  virtual ~CapsLockDelegate() {}

  // A derived class should do either of the following: 1) toggle Caps Lock and
  // return true, or 2) do nothing and return false (see crosbug.com/110127).
  virtual bool HandleToggleCapsLock() = 0;
};
}  // namespace ash

#endif  // ASH_CAPS_LOCK_DELEGATE_H_
