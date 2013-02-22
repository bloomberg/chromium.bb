// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TAB_MODAL_CONFIRM_DIALOG_H_
#define CHROME_BROWSER_UI_TAB_MODAL_CONFIRM_DIALOG_H_

#include "chrome/browser/ui/tab_modal_confirm_dialog_delegate.h"

namespace content {
class WebContents;
}

// Base class for the tab modal confirm dialog.
class TabModalConfirmDialog : public TabModalConfirmDialogCloseDelegate {
 public:
  // Platform specific factory function. This function will automatically show
  // the dialog.
  static TabModalConfirmDialog* Create(TabModalConfirmDialogDelegate* delegate,
                                       content::WebContents* web_contents);
  // Accepts the dialog.
  virtual void AcceptTabModalDialog() = 0;

  // Cancels the dialog.
  virtual void CancelTabModalDialog() = 0;

  // TabModalConfirmDialogCloseDelegate:
  // Closes the dialog.
  virtual void CloseDialog() = 0;

 protected:
  virtual ~TabModalConfirmDialog() {}
};

#endif  // CHROME_BROWSER_UI_TAB_MODAL_CONFIRM_DIALOG_H_
