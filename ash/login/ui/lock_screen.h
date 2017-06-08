// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOCK_SCREEN_H_
#define ASH_LOGIN_UI_LOCK_SCREEN_H_

#include "ash/ash_export.h"

namespace ash {

// TODO(jdufault): There's some internal state here so put ShowLockScreen and
// DestroyLockScreen inside a (static) class, ie, ash::LockScreen::Show() and
// ash::LockScreen::Destroy().

// Creates and displays the lock screen. Returns true if the lock screen was
// displayed.
//
// The lock screen communicates with the backend C++ via a mojo API.
ASH_EXPORT bool ShowLockScreen();

// Destroys the lock screen. There must be an existing lock screen instance.
ASH_EXPORT void DestroyLockScreen();

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOCK_SCREEN_H_
