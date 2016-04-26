// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_COMMON_WM_WINDOW_TRACKER_H_
#define ASH_WM_COMMON_WM_WINDOW_TRACKER_H_

#include "ash/wm/common/wm_window.h"
#include "ash/wm/common/wm_window_observer.h"
#include "ui/base/window_tracker_template.h"

namespace ash {
namespace wm {

using WmWindowTracker = ui::WindowTrackerTemplate<WmWindow, WmWindowObserver>;

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_COMMON_WM_WINDOW_TRACKER_H_
