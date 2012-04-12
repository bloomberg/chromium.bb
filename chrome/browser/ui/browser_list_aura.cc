// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_list.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_ui_manager.h"

#if defined(USE_ASH)
#include "ash/shell.h"
#endif

// static
void BrowserList::HandleAppExitingForPlatform() {
  // Close All non browser windows now. Those includes notifications
  // and windows created by Ash (launcher, background, etc).
  g_browser_process->notification_ui_manager()->CancelAll();
#if defined(USE_ASH)
  // Delete the shell while the message loop is still alive.
  ash::Shell::DeleteInstance();
#endif
#if defined(OS_CHROMEOS)
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableZeroBrowsersOpenForTests)) {
    // App is exiting, call EndKeepAlive() on behalf of Aura Shell.
    BrowserList::EndKeepAlive();
    // Make sure we have notified the session manager that we are exiting.
    // This might be called from FastShutdown() or CloseAllBrowsers(), but not
    // if something prevents a browser from closing before SetTryingToQuit()
    // gets called (e.g. browser->TabsNeedBeforeUnloadFired() is true).
    // NotifyAndTerminate does nothing if called more than once.
    BrowserList::NotifyAndTerminate(true);
  }
#endif // OS_CHROMEOS
}
