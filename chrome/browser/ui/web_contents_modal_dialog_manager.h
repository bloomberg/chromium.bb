// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_CONTENTS_MODAL_DIALOG_MANAGER_H_
#define CHROME_BROWSER_UI_WEB_CONTENTS_MODAL_DIALOG_MANAGER_H_

#include <deque>

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class WebContentsModalDialog;
class WebContentsModalDialogManagerDelegate;

// Per-WebContents class to manage WebContents-modal dialogs.
class WebContentsModalDialogManager
    : public content::WebContentsObserver,
      public content::WebContentsUserData<WebContentsModalDialogManager> {
 public:
  virtual ~WebContentsModalDialogManager();

  WebContentsModalDialogManagerDelegate* delegate() const { return delegate_; }
  void set_delegate(WebContentsModalDialogManagerDelegate* d) { delegate_ = d; }

  // Adds the given dialog to the list of child dialogs. The dialog will notify
  // via WillClose() when it is being destroyed.
  void AddDialog(WebContentsModalDialog* dialog);

  // Called when a WebContentsModalDialogs we own is about to be closed.
  void WillClose(WebContentsModalDialog* dialog);

  // Blocks/unblocks interaction with renderer process.
  void BlockWebContentsInteraction(bool blocked);

  // Returns true if a dialog is currently being shown.
  bool IsShowingDialog() const;

  // Focus the topmost modal dialog.  IsShowingDialog() must be true when
  // calling this function.
  void FocusTopmostDialog();

  // For testing.
  class TestApi {
   public:
    explicit TestApi(WebContentsModalDialogManager* manager)
        : manager_(manager) {}

    void CloseAllDialogs() { manager_->CloseAllDialogs(); }

   private:
    WebContentsModalDialogManager* manager_;

    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

 private:
  explicit WebContentsModalDialogManager(content::WebContents* web_contents);
  friend class content::WebContentsUserData<WebContentsModalDialogManager>;

  typedef std::deque<WebContentsModalDialog*> WebContentsModalDialogList;

  // Returns the number of dialogs in this tab.
  size_t dialog_count() const { return child_dialogs_.size(); }

  // Return an iterator for the first dialog in this web contents.
  WebContentsModalDialogList::iterator dialog_begin() {
    return child_dialogs_.begin();
  }

  // Return an iterator for the last dialog in this web contents.
  WebContentsModalDialogList::iterator dialog_end() {
    return child_dialogs_.end();
  }

  // Closes all WebContentsModalDialogs.
  void CloseAllDialogs();

  // Overridden from content::WebContentsObserver:
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;
  virtual void DidGetIgnoredUIEvent() OVERRIDE;
  virtual void WebContentsDestroyed(content::WebContents* tab) OVERRIDE;

  // Delegate for notifying our owner about stuff. Not owned by us.
  WebContentsModalDialogManagerDelegate* delegate_;

  // All active dialogs.
  WebContentsModalDialogList child_dialogs_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsModalDialogManager);
};

#endif  // CHROME_BROWSER_UI_WEB_CONTENTS_MODAL_DIALOG_MANAGER_H_
