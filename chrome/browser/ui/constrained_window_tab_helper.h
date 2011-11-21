// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CONSTRAINED_WINDOW_TAB_HELPER_H_
#define CHROME_BROWSER_UI_CONSTRAINED_WINDOW_TAB_HELPER_H_
#pragma once

#include <deque>

#include "content/browser/tab_contents/tab_contents_observer.h"

class ConstrainedWindow;
class ConstrainedWindowTabHelperDelegate;
class TabContentsWrapper;

// Per-tab class to manage constrained windows.
class ConstrainedWindowTabHelper : public TabContentsObserver {
 public:
  explicit ConstrainedWindowTabHelper(TabContentsWrapper* tab_contents);
  virtual ~ConstrainedWindowTabHelper();

  ConstrainedWindowTabHelperDelegate* delegate() const { return delegate_; }
  void set_delegate(ConstrainedWindowTabHelperDelegate* d) { delegate_ = d; }

  // Adds the given window to the list of child windows. The window will notify
  // via WillClose() when it is being destroyed.
  void AddConstrainedDialog(ConstrainedWindow* window);

  // Closes all constrained windows.
  void CloseConstrainedWindows();

  // Called when a ConstrainedWindow we own is about to be closed.
  void WillClose(ConstrainedWindow* window);

  // Blocks/unblocks interaction with renderer process.
  void BlockTabContent(bool blocked);

  // Returns the number of constrained windows in this tab.  Used by tests.
  size_t constrained_window_count() { return child_windows_.size(); }

  typedef std::deque<ConstrainedWindow*> ConstrainedWindowList;

  // Return an iterator for the first constrained window in this tab contents.
  ConstrainedWindowList::iterator constrained_window_begin() {
    return child_windows_.begin();
  }

  // Return an iterator for the last constrained window in this tab contents.
  ConstrainedWindowList::iterator constrained_window_end() {
    return child_windows_.end();
  }

 private:
  // Overridden from TabContentsObserver:
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;
  virtual void DidGetIgnoredUIEvent() OVERRIDE;
  virtual void TabContentsDestroyed(TabContents* tab) OVERRIDE;

  // Our owning TabContentsWrapper.
  TabContentsWrapper* wrapper_;

  // Delegate for notifying our owner about stuff. Not owned by us.
  ConstrainedWindowTabHelperDelegate* delegate_;

  // All active constrained windows.
  ConstrainedWindowList child_windows_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedWindowTabHelper);
};

#endif  // CHROME_BROWSER_UI_CONSTRAINED_WINDOW_TAB_HELPER_H_
