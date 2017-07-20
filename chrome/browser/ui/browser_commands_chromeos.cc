// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_commands_chromeos.h"

#include "ash/accelerators/accelerator_controller_delegate_classic.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/screenshot_delegate.h"
#include "ash/shell_port_classic.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"

using base::UserMetricsAction;

void TakeScreenshot() {
  base::RecordAction(UserMetricsAction("Menu_Take_Screenshot"));
  ash::ScreenshotDelegate* screenshot_delegate =
      ash::ShellPortClassic::Get()
          ->accelerator_controller_delegate()
          ->screenshot_delegate();
  if (screenshot_delegate &&
      screenshot_delegate->CanTakeScreenshot()) {
    screenshot_delegate->HandleTakeScreenshotForAllRootWindows();
  }
}
