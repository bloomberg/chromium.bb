// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/application_lifetime.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_ui_manager.h"

namespace browser {

void HandleAppExitingForPlatform() {
  // Close All non browser windows now. Those includes notifications
  // and windows created by Ash (launcher, background, etc).
  g_browser_process->notification_ui_manager()->CancelAll();
  // TODO(oshima): Close all non browser windows here while
  // the message loop is still alive.
#if defined(OS_CHROMEOS)
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableZeroBrowsersOpenForTests)) {
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

}  // namespace browser
