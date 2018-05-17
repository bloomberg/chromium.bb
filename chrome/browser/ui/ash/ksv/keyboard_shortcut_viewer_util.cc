// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/ksv/keyboard_shortcut_viewer_util.h"

#include "ash/components/shortcut_viewer/views/keyboard_shortcut_view.h"
#include "ash/shell.h"

namespace keyboard_shortcut_viewer_util {

void ShowKeyboardShortcutViewer() {
  // TODO(https://crbug.com/833673): Remove the dependency on aura::Window.
  // TODO(https://crbug.com/764009): GetRootWindowForNewWindows Mash support.
  keyboard_shortcut_viewer::KeyboardShortcutView::Show(
      ash::Shell::HasInstance() ? ash::Shell::GetRootWindowForNewWindows()
                                : nullptr);
}

}  // namespace keyboard_shortcut_viewer_util
