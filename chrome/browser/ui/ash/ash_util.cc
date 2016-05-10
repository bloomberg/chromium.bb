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
  return !IsRunningInMash();
}

bool IsRunningInMash() {
#if defined(MOJO_SHELL_CLIENT)
  return content::MojoShellConnection::Get() &&
         content::MojoShellConnection::Get()->UsingExternalShell();
#else
  return false;
#endif
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
