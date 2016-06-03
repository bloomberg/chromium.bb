// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WM_WINDOW_TRACKER_H_
#define ASH_COMMON_WM_WINDOW_TRACKER_H_

#include "ash/common/wm_window.h"
#include "ash/common/wm_window_observer.h"
#include "ui/base/window_tracker_template.h"

namespace ash {

using WmWindowTracker = ui::WindowTrackerTemplate<WmWindow, WmWindowObserver>;

}  // namespace ash

#endif  // ASH_COMMON_WM_WINDOW_TRACKER_H_
