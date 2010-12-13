// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_closeable_state_watcher.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/tab_closeable_state_watcher.h"
#endif  // defined(OS_CHROMEOS)

////////////////////////////////////////////////////////////////////////////////
// TabCloseableStateWatcher, static:

::TabCloseableStateWatcher* ::TabCloseableStateWatcher::Create() {
  ::TabCloseableStateWatcher* watcher = NULL;
#if defined(OS_CHROMEOS)
  // We only watch closeable state of tab on chromeos, and only when it's not
  // disabled (tests will have the disable switch).
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableTabCloseableStateWatcher))
    watcher = new chromeos::TabCloseableStateWatcher();
#endif  // OS_CHROMEOS
  if (!watcher)
    watcher = new ::TabCloseableStateWatcher();
  return watcher;
}

bool TabCloseableStateWatcher::CanCloseTab(const Browser* browser) const {
  return true;
}

bool TabCloseableStateWatcher::CanCloseBrowser(Browser* browser) {
  return true;
}
