// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/application_lifetime.h"

#include "ash/shell.h"
#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/common/chrome_switches.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/views/widget/widget.h"

namespace chrome {

void HandleAppExitingForPlatform() {
  // Close all non browser windows now. Those includes notifications
  // and windows created by Ash (launcher, background, etc).
  g_browser_process->notification_ui_manager()->CancelAll();

  // This may be called before |ash::Shell| is initialized when
  // XIOError is reported.  crbug.com/150633.
  if (ash::Shell::HasInstance()) {
    // Releasing the capture will close any menus that might be open:
    // http://crbug.com/134472
    aura::client::GetCaptureClient(ash::Shell::GetPrimaryRootWindow())->
        SetCapture(NULL);
  }

  views::Widget::CloseAllSecondaryWidgets();

#if defined(OS_CHROMEOS)
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableZeroBrowsersOpenForTests)) {
    // App is exiting, call DecrementKeepAliveCount() on behalf of Aura Shell.
    DecrementKeepAliveCount();
    // Make sure we have notified the session manager that we are exiting.
    // This might be called from FastShutdown() or CloseAllBrowsers(), but not
    // if something prevents a browser from closing before SetTryingToQuit()
    // gets called (e.g. browser->TabsNeedBeforeUnloadFired() is true).
    // NotifyAndTerminate does nothing if called more than once.
    NotifyAndTerminate(true);
  }
#endif  // OS_CHROMEOS
}

}  // namespace chrome
