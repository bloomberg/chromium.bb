// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOCK_SCREEN_H_
#define ASH_LOGIN_UI_LOCK_SCREEN_H_

#include "ash/ash_export.h"

namespace ash {

// Creates and displays the lock screen. Returns true if the lock screen was
// displayed.
//
// The lock screen communicates with the backend C++ via a mojo API.
ASH_EXPORT bool ShowLockScreen();

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOCK_SCREEN_H_
