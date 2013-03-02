// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/ash_util.h"

#include "ash/shell.h"
#include "chrome/browser/ui/ash/ash_init.h"
#include "chrome/browser/ui/host_desktop.h"
#include "ui/aura/root_window.h"

namespace chrome {

bool IsNativeViewInAsh(gfx::NativeView native_view) {
#if defined(OS_CHROMEOS)
  // Optimization. There is only ash on ChromeOS.
  return true;
#endif

  if (!ash::Shell::HasInstance())
    return false;

  ash::Shell::RootWindowList root_windows =
      ash::Shell::GetInstance()->GetAllRootWindows();

  for (ash::Shell::RootWindowList::const_iterator it = root_windows.begin();
       it != root_windows.end(); ++it) {
    if ((*it)->Contains(native_view))
      return true;
  }

  return false;
}

bool IsNativeWindowInAsh(gfx::NativeWindow native_window) {
  return IsNativeViewInAsh(native_window);
}

void ToggleAshDesktop() {
  if (chrome::HOST_DESKTOP_TYPE_ASH == chrome::HOST_DESKTOP_TYPE_NATIVE)
    return;

  if (!ash::Shell::HasInstance())
    OpenAsh();
  else
    CloseAsh();
}

}  // namespace chrome
