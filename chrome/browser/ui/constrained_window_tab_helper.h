// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CONSTRAINED_WINDOW_TAB_HELPER_H_
#define CHROME_BROWSER_UI_CONSTRAINED_WINDOW_TAB_HELPER_H_

#include <deque>

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class ConstrainedWindow;
class ConstrainedWindowTabHelperDelegate;

// Per-tab class to manage constrained windows.
class ConstrainedWindowTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ConstrainedWindowTabHelper> {
 public:
  virtual ~ConstrainedWindowTabHelper();

  ConstrainedWindowTabHelperDelegate* delegate() const { return delegate_; }
  void set_delegate(ConstrainedWindowTabHelperDelegate* d) { delegate_ = d; }

  // Adds the given window to the list of child windows. The window will notify
  // via WillClose() when it is being destroyed.
  void AddDialog(ConstrainedWindow* window);

  // Closes all WebContentsModalDialogs.
  void CloseAllDialogs();

  // Called when a WebContentsModalDialogs we own is about to be closed.
  void WillClose(ConstrainedWindow* window);

  // Blocks/unblocks interaction with renderer process.
  void BlockWebContentsInteraction(bool blocked);

  // Returns the number of constrained windows in this tab.
  size_t dialog_count() { return child_dialogs_.size(); }

  typedef std::deque<ConstrainedWindow*> WebContentsModalDialogList;

  // Return an iterator for the first constrained window in this web contents.
  WebContentsModalDialogList::iterator dialog_begin() {
    return child_dialogs_.begin();
  }

  // Return an iterator for the last constrained window in this web contents.
  WebContentsModalDialogList::iterator dialog_end() {
    return child_dialogs_.end();
  }

 private:
  explicit ConstrainedWindowTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<ConstrainedWindowTabHelper>;

  // Overridden from content::WebContentsObserver:
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;
  virtual void DidGetIgnoredUIEvent() OVERRIDE;
  virtual void WebContentsDestroyed(content::WebContents* tab) OVERRIDE;

  // Delegate for notifying our owner about stuff. Not owned by us.
  ConstrainedWindowTabHelperDelegate* delegate_;

  // All active constrained windows.
  WebContentsModalDialogList child_dialogs_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedWindowTabHelper);
};

#endif  // CHROME_BROWSER_UI_CONSTRAINED_WINDOW_TAB_HELPER_H_
