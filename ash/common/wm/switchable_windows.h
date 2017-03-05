// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WM_SWITCHABLE_WINDOWS_H_
#define ASH_COMMON_WM_SWITCHABLE_WINDOWS_H_

#include <stddef.h>

#include "ash/ash_export.h"
#include "base/macros.h"

namespace ash {

class WmWindow;

namespace wm {

// List of containers which contain windows that can be switched via Alt+Tab to.
ASH_EXPORT extern const int kSwitchableWindowContainerIds[];

// The number of elements in kSwitchableWindowContainerIds.
ASH_EXPORT extern const size_t kSwitchableWindowContainerIdsLength;

// Returns true if |window| is a container for windows which can be switched to.
ASH_EXPORT bool IsSwitchableContainer(const WmWindow* window);

}  // namespace wm
}  // namespace ash

#endif  // ASH_COMMON_WM_SWITCHABLE_WINDOWS_H_
