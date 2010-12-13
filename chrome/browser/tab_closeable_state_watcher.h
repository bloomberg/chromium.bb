// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CLOSEABLE_STATE_WATCHER_H_
#define CHROME_BROWSER_TAB_CLOSEABLE_STATE_WATCHER_H_
#pragma once

#include "base/basictypes.h"

class Browser;

// Base class for watching closeable state of tabs, which always returns true
// to allow closing for all tabs and browsers.  Classes that desire otherwise
// can inherit this class and override the methods as deemed appropriate to
// allow or disallow tabs or browsers to be closed.

class TabCloseableStateWatcher {
 public:
  TabCloseableStateWatcher() {}
  virtual ~TabCloseableStateWatcher() {}

  // Creates the appropriate TabCloseableStateWatcher.  The caller owns the
  // returned object.
  static TabCloseableStateWatcher* Create();

  // Called from Browser::CanCloseTab which overrides TabStripModelDelegate::
  // CanCloseTab, and returns true if tab of browser is closeable.
  // Browser::CanCloseTab is in turn called by Browser, TabStripModel and
  // BrowserTabStripController when:
  // - rendering tab to determine if close button should be drawn
  // - setting up tab's context menu to determine if "Close tab" should
  //   should be enabled
  // - determining if accelerator keys to close tab should be processed
  virtual bool CanCloseTab(const Browser* browser) const;

  // Called from Browser::IsClosingPermitted which is in turn called from
  // Browser::ShouldCloseWindow to check if |browser| can be closed.
  virtual bool CanCloseBrowser(Browser* browser);

  // Called from Browser::CancelWindowClose when closing of window is canceled.
  // Watcher is potentially interested in this, especially when the closing of
  // window was initiated by application exiting.
  virtual void OnWindowCloseCanceled(Browser* browser) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TabCloseableStateWatcher);
};

#endif  // CHROME_BROWSER_TAB_CLOSEABLE_STATE_WATCHER_H_
