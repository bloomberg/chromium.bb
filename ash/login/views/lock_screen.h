// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_VIEWS_LOCK_SCREEN_H_
#define ASH_LOGIN_VIEWS_LOCK_SCREEN_H_

#include "ash/ash_export.h"

namespace views {
class Widget;
}

namespace ash {

// Fills the given widget with the lock screen view and displays it.
//
// The lock screen communicates with the backend C++ via a mojo API.
ASH_EXPORT void ShowLockScreenInWidget(views::Widget* widget);

}  // namespace ash

#endif  // ASH_LOGIN_VIEWS_LOCK_SCREEN_H_
