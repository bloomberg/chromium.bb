// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/ash_util.h"

#include "ash/accelerators/accelerator_controller.h"
#include "ash/shell.h"
#include "build/build_config.h"
#include "chrome/browser/ui/ash/ash_init.h"
#include "ui/aura/window_event_dispatcher.h"

#if defined(MOJO_SHELL_CLIENT)
#include "content/public/common/mojo_shell_connection.h"
#endif

namespace chrome {

bool ShouldOpenAshOnStartup() {
#if defined(OS_CHROMEOS)
  return !IsRunningInMash();
#else
  return false;
#endif
}

bool IsRunningInMash() {
#if defined(OS_CHROMEOS) && defined(MOJO_SHELL_CLIENT)
  return content::MojoShellConnection::Get() &&
         content::MojoShellConnection::Get()->UsingExternalShell();
#else
  return false;
#endif
}

bool IsNativeViewInAsh(gfx::NativeView native_view) {
#if defined(OS_CHROMEOS)
  // Optimization. There is only ash on ChromeOS.
  return true;
#endif

  if (!ash::Shell::HasInstance())
    return false;

  aura::Window::Windows root_windows =
      ash::Shell::GetInstance()->GetAllRootWindows();

  for (aura::Window::Windows::const_iterator it = root_windows.begin();
       it != root_windows.end(); ++it) {
    if ((*it)->Contains(native_view))
      return true;
  }

  return false;
}

bool IsNativeWindowInAsh(gfx::NativeWindow native_window) {
  return IsNativeViewInAsh(native_window);
}

bool IsAcceleratorDeprecated(const ui::Accelerator& accelerator) {
  // When running in mash the browser doesn't handle ash accelerators.
  if (chrome::IsRunningInMash())
    return false;

  ash::AcceleratorController* controller =
      ash::Shell::GetInstance()->accelerator_controller();
  return controller->IsDeprecated(accelerator);
}

}  // namespace chrome
