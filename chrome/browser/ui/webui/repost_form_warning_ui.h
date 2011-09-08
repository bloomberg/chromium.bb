// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_REPOST_FORM_WARNING_UI_H_
#define CHROME_BROWSER_UI_WEBUI_REPOST_FORM_WARNING_UI_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "ui/gfx/native_widget_types.h"

class BrowserWindow;
class RepostFormWarningController;
class TabContents;

namespace browser {
void ShowRepostFormWarningDialog(gfx::NativeWindow parent_window,
                                 TabContents* tab_contents);
}

// Displays a dialog that warns the user that they are about to resubmit
// a form.
// To display the dialog, allocate this object on the heap. It will open the
// dialog from its constructor and then delete itself when the user dismisses
// the dialog.
class RepostFormWarningUI {
 public:
  // Invoked when the dialog is closed.  Notifies the controller of the user's
  // response and deletes this object.
  void OnDialogClosed(bool repost);

 private:
  friend void browser::ShowRepostFormWarningDialog(
      gfx::NativeWindow parent_window, TabContents* tab_contents);

  // Call BrowserWindow::ShowRepostFormWarningDialog() to use.
  RepostFormWarningUI(gfx::NativeWindow parent_window,
                      TabContents* tab_contents);

  virtual ~RepostFormWarningUI();

  scoped_ptr<RepostFormWarningController> controller_;

  DISALLOW_COPY_AND_ASSIGN(RepostFormWarningUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_REPOST_FORM_WARNING_UI_H_
