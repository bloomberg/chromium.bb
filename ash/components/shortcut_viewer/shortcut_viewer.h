// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_SHORTCUT_VIEWER_SHORTCUT_VIEWER_H_
#define ASH_COMPONENTS_SHORTCUT_VIEWER_SHORTCUT_VIEWER_H_

#include "base/time/time.h"

namespace keyboard_shortcut_viewer {

// Toggle the Keyboard Shortcut Viewer window. |user_gesture_time| is the time
// of the user gesture that caused the window to show. Used for metrics.
void Toggle(base::TimeTicks user_gesture_time);

}  // namespace keyboard_shortcut_viewer

#endif  // ASH_COMPONENTS_SHORTCUT_VIEWER_SHORTCUT_VIEWER_H_
