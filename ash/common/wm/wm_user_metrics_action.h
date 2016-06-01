// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WM_WM_USER_METRICS_ACTION_H_
#define ASH_COMMON_WM_WM_USER_METRICS_ACTION_H_

namespace ash {
namespace wm {

enum class WmUserMetricsAction {
  DRAG_MAXIMIZE_LEFT,
  DRAG_MAXIMIZE_RIGHT,
  // Thumbnail sized overview of windows triggered by pressing the overview key.
  WINDOW_OVERVIEW,

  // User selected a window in overview mode different from the
  // previously-active window.
  WINDOW_OVERVIEW_ACTIVE_WINDOW_CHANGED,

  // Selecting a window in overview mode by pressing the enter key.
  WINDOW_OVERVIEW_ENTER_KEY,
};

}  // namespace wm
}  // namespace ash

#endif  // ASH_COMMON_WM_WM_USER_METRICS_ACTION_H_
