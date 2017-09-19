// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_commands_chromeos.h"

#include "ash/accelerators/accelerator_controller_delegate_classic.h"  // mash-ok
#include "ash/mus/bridge/shell_port_mash.h"  // mash-ok
#include "ash/public/cpp/config.h"
#include "ash/screenshot_delegate.h"
#include "ash/shell_port_classic.h"  // mash-ok
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/chromeos/ash_config.h"

using base::UserMetricsAction;

void TakeScreenshot() {
  base::RecordAction(UserMetricsAction("Menu_Take_Screenshot"));
  ash::AcceleratorControllerDelegateClassic* accelerator_controller_delegate =
      nullptr;
  if (chromeos::GetAshConfig() == ash::Config::CLASSIC) {
    accelerator_controller_delegate =
        ash::ShellPortClassic::Get()->accelerator_controller_delegate();
  } else if (chromeos::GetAshConfig() == ash::Config::MUS) {
    accelerator_controller_delegate =
        ash::mus::ShellPortMash::Get()->accelerator_controller_delegate_mus();
  } else {
    // TODO(mash): Screenshot support. http://crbug.com/557397
    NOTIMPLEMENTED();
    return;
  }
  ash::ScreenshotDelegate* screenshot_delegate =
      accelerator_controller_delegate->screenshot_delegate();
  if (screenshot_delegate && screenshot_delegate->CanTakeScreenshot())
    screenshot_delegate->HandleTakeScreenshotForAllRootWindows();
}
