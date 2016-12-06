// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/ash_util.h"

#include "ash/common/accelerators/accelerator_controller.h"
#include "ash/common/wm_shell.h"
#include "build/build_config.h"
#include "chrome/browser/ui/ash/ash_init.h"
#include "content/public/common/service_names.mojom.h"
#include "services/service_manager/runner/common/client_util.h"
#include "ui/aura/window_event_dispatcher.h"

namespace ash_util {

const char* GetAshServiceName() {
  // Under mash the ash process provides the service.
  if (chrome::IsRunningInMash())
    return "ash";

  // Under classic ash the browser process provides the service.
  return content::mojom::kBrowserServiceName;
}

}  // namespace ash_util

namespace chrome {

bool ShouldOpenAshOnStartup() {
  return !IsRunningInMash();
}

bool IsRunningInMash() {
  return service_manager::ServiceManagerIsRemote();
}

bool IsAcceleratorDeprecated(const ui::Accelerator& accelerator) {
  // When running in mash the browser doesn't handle ash accelerators.
  if (chrome::IsRunningInMash())
    return false;

  return ash::WmShell::Get()->accelerator_controller()->IsDeprecated(
      accelerator);
}

}  // namespace chrome
