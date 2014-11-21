// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_TAB_LOADER_DELEGATE_H_
#define CHROME_BROWSER_SESSIONS_TAB_LOADER_DELEGATE_H_

#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"

class TabLoaderCallback {
 public:
  // This function will get called to suppress and to allow tab loading. Tab
  // loading is initially enabled.
  virtual void SetTabLoadingEnabled(bool enable_tab_loading) = 0;
};

// TabLoaderDelegate is created once the SessionRestore process is complete and
// the loading of hidden tabs starts.
class TabLoaderDelegate {
 public:
  TabLoaderDelegate() {}
  virtual ~TabLoaderDelegate() {}

  // Create a tab loader delegate. |TabLoaderCallback::SetTabLoadingEnabled| can
  // get called to disable / enable tab loading.
  // The callback object is valid as long as this object exists.
  static scoped_ptr<TabLoaderDelegate> Create(TabLoaderCallback* callback);

  // Returns the default timeout time after which the next tab gets loaded if
  // the previous tab did not finish to load.
  virtual base::TimeDelta GetTimeoutBeforeLoadingNextTab() const = 0;
};

#endif  // CHROME_BROWSER_SESSIONS_TAB_LOADER_DELEGATE_H_
