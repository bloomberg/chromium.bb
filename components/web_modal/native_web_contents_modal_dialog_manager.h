// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_MODAL_NATIVE_WEB_CONTENTS_MODAL_DIALOG_MANAGER_H_
#define COMPONENTS_WEB_MODAL_NATIVE_WEB_CONTENTS_MODAL_DIALOG_MANAGER_H_

#include "components/web_modal/native_web_contents_modal_dialog.h"

namespace content {
class WebContents;
}  // namespace content

namespace web_modal {

class WebContentsModalDialogHost;

// Interface from NativeWebContentsModalDialogManager to
// WebContentsModalDialogManager.
class NativeWebContentsModalDialogManagerDelegate {
 public:
  NativeWebContentsModalDialogManagerDelegate() {}
  virtual ~NativeWebContentsModalDialogManagerDelegate() {}

  virtual content::WebContents* GetWebContents() const = 0;

  // Notify the delegate that the dialog is closing. The native
  // manager will be deleted before the end of this call.
  virtual void WillClose(NativeWebContentsModalDialog dialog) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeWebContentsModalDialogManagerDelegate);
};

// Provides an interface for platform-specific UI implementation for the web
// contents modal dialog.
// TODO(gbillock): perhaps rename to "SingleWebContentsDialogManager"
class NativeWebContentsModalDialogManager {
 public:
  virtual ~NativeWebContentsModalDialogManager() {}

  // Starts management of the modal aspects of the dialog.  This function should
  // also register to be notified when the dialog is closing, so that it can
  // notify the manager.
  virtual void ManageDialog(NativeWebContentsModalDialog dialog) = 0;

  // Makes the web contents modal dialog visible. Only one web contents modal
  // dialog is shown at a time per tab.
  virtual void ShowDialog(NativeWebContentsModalDialog dialog) = 0;

  // Hides the web contents modal dialog without closing it.
  virtual void HideDialog(NativeWebContentsModalDialog dialog) = 0;

  // Closes the web contents modal dialog.
  // If this method causes a WillClose() call to the delegate, the manager
  // will be deleted at the close of that invocation.
  virtual void CloseDialog(NativeWebContentsModalDialog dialog) = 0;

  // Sets focus on the web contents modal dialog.
  virtual void FocusDialog(NativeWebContentsModalDialog dialog) = 0;

  // Runs a pulse animation for the web contents modal dialog.
  virtual void PulseDialog(NativeWebContentsModalDialog dialog) = 0;

  // Called when the host view for the dialog has changed.
  virtual void HostChanged(WebContentsModalDialogHost* new_host) = 0;

 protected:
  NativeWebContentsModalDialogManager() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeWebContentsModalDialogManager);
};

}  // namespace web_modal

#endif  // COMPONENTS_WEB_MODAL_NATIVE_WEB_CONTENTS_MODAL_DIALOG_MANAGER_H_
