// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/application_lifetime.h"

#include "base/command_line.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/switch_utils.h"
#include "ui/views/widget/widget.h"

#if defined(USE_ASH)
#include "ash/shell.h"
#include "ui/aura/client/capture_client.h"
#endif

#if defined(OS_CHROMEOS)
#include "chromeos/display/output_configurator.h"
#endif

namespace chrome {

void HandleAppExitingForPlatform() {
  // Close all non browser windows now. Those includes notifications
  // and windows created by Ash (launcher, background, etc).
#if defined(USE_ASH)
  // This may be called before |ash::Shell| is initialized when
  // XIOError is reported.  crbug.com/150633.
  if (ash::Shell::HasInstance()) {
    g_browser_process->notification_ui_manager()->CancelAll();
    // Releasing the capture will close any menus that might be open:
    // http://crbug.com/134472
    aura::client::GetCaptureClient(ash::Shell::GetPrimaryRootWindow())->
        SetCapture(NULL);
  }
#else
  g_browser_process->notification_ui_manager()->CancelAll();
#endif

  views::Widget::CloseAllSecondaryWidgets();

#if defined(OS_CHROMEOS)
  // Stop handling display configuration events once the shutdown
  // process starts. crbug.com/177014.
  ash::Shell::GetInstance()->output_configurator()->Stop();

  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableZeroBrowsersOpenForTests) &&
      !chrome::IsRunningInAppMode()) {
    // App is exiting, call EndKeepAlive() on behalf of Aura Shell.
    EndKeepAlive();
    // Make sure we have notified the session manager that we are exiting.
    // This might be called from FastShutdown() or CloseAllBrowsers(), but not
    // if something prevents a browser from closing before SetTryingToQuit()
    // gets called (e.g. browser->TabsNeedBeforeUnloadFired() is true).
    // NotifyAndTerminate does nothing if called more than once.
    NotifyAndTerminate(true);
  }
#endif // OS_CHROMEOS
}

}  // namespace chrome
