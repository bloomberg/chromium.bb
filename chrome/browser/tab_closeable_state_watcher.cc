// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_closeable_state_watcher.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"

////////////////////////////////////////////////////////////////////////////////
// TabCloseableStateWatcher, static:

::TabCloseableStateWatcher* ::TabCloseableStateWatcher::Create() {
  return new ::TabCloseableStateWatcher();
}

bool TabCloseableStateWatcher::CanCloseTab(const Browser* browser) const {
  return true;
}

bool TabCloseableStateWatcher::CanCloseTabs(const Browser* browser,
    std::vector<int>* indices) const {
  return true;
}

bool TabCloseableStateWatcher::CanCloseBrowser(Browser* browser) {
  return true;
}
