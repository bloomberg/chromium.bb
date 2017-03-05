// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WM_FOCUS_RULES_H_
#define ASH_COMMON_WM_FOCUS_RULES_H_

#include "ash/ash_export.h"

namespace ash {

class WmWindow;

// These functions provide the ash implementation wm::FocusRules. See
// description there for details.
ASH_EXPORT bool IsToplevelWindow(WmWindow* window);
ASH_EXPORT bool IsWindowConsideredActivatable(WmWindow* window);
ASH_EXPORT bool IsWindowConsideredVisibleForActivation(WmWindow* window);

}  // namespace ash

#endif  // ASH_COMMON_WM_FOCUS_RULES_H_
