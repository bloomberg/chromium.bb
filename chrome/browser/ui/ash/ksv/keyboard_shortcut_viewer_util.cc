// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/ksv/keyboard_shortcut_viewer_util.h"

#include "ash/components/shortcut_viewer/views/keyboard_shortcut_view.h"
#include "ash/wm/window_util.h"

namespace keyboard_shortcut_viewer_util {

void ShowKeyboardShortcutViewer() {
  keyboard_shortcut_viewer::KeyboardShortcutView::Show(
      ash::wm::GetActiveWindow());
}

}  // namespace keyboard_shortcut_viewer_util
