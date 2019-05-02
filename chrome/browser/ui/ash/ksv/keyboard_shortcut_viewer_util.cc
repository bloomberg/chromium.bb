// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/ksv/keyboard_shortcut_viewer_util.h"

#include "ash/components/shortcut_viewer/shortcut_viewer.h"
#include "base/time/time.h"

namespace keyboard_shortcut_viewer_util {

void ToggleKeyboardShortcutViewer() {
  keyboard_shortcut_viewer::Toggle(base::TimeTicks::Now());
}

}  // namespace keyboard_shortcut_viewer_util
