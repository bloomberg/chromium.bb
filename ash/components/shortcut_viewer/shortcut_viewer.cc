// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/shortcut_viewer/shortcut_viewer.h"

#include "ash/components/shortcut_viewer/views/keyboard_shortcut_view.h"

namespace keyboard_shortcut_viewer {

void Toggle(base::TimeTicks user_gesture_time) {
  KeyboardShortcutView::Toggle(user_gesture_time, nullptr);
}

}  // namespace keyboard_shortcut_viewer
