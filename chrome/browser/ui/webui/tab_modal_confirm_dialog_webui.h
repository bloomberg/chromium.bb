// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TAB_MODAL_CONFIRM_DIALOG_WEBUI_H_
#define CHROME_BROWSER_UI_WEBUI_TAB_MODAL_CONFIRM_DIALOG_WEBUI_H_
#pragma once

#if !(defined(USE_AURA) || defined(TOOLKIT_VIEWS))
#error Tab-modal confirm dialog should be shown with native UI.
#endif

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

class TabContentsWrapper;
class TabModalConfirmDialogDelegate;

// Displays a tab-modal dialog, i.e. a dialog that will block the current page
// but still allow the user to switch to a different page.
// To display the dialog, allocate this object on the heap. It will open the
// dialog from its constructor and then delete itself when the user dismisses
// the dialog.
class TabModalConfirmDialogUI {
 public:
  TabModalConfirmDialogUI(TabModalConfirmDialogDelegate* delegate,
                          TabContentsWrapper* wrapper);
  ~TabModalConfirmDialogUI();

  // Invoked when the dialog is closed. Notifies the controller of the user's
  // response.
  void OnDialogClosed(bool accept);

 private:
  scoped_ptr<TabModalConfirmDialogDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(TabModalConfirmDialogUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_TAB_MODAL_CONFIRM_DIALOG_WEBUI_H_
