// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_NATIVE_WEB_CONTENTS_MODAL_DIALOG_MANAGER_H_
#define CHROME_BROWSER_UI_NATIVE_WEB_CONTENTS_MODAL_DIALOG_MANAGER_H_

#include "chrome/browser/ui/native_web_contents_modal_dialog.h"

// Provides an interface for platform-specific UI implementation for the web
// contents modal dialog.
class NativeWebContentsModalDialogManager {
 public:
  virtual ~NativeWebContentsModalDialogManager() {}

  // Starts management of the modal aspects of the dialog.  This function should
  // also register to be notified when the dialog is closing, so that it can
  // notify the manager.
  virtual void ManageDialog(NativeWebContentsModalDialog dialog) = 0;

  // Closes the web contents modal dialog.
  virtual void CloseDialog(NativeWebContentsModalDialog dialog) = 0;

 protected:
  NativeWebContentsModalDialogManager() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeWebContentsModalDialogManager);
};

#endif  // CHROME_BROWSER_UI_NATIVE_WEB_CONTENTS_MODAL_DIALOG_MANAGER_H_
