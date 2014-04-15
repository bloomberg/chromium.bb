// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_modal/web_contents_modal_dialog_manager.h"

#include "chrome/browser/ui/cocoa/constrained_window/constrained_window_mac.h"
#include "components/web_modal/single_web_contents_dialog_manager.h"

using web_modal::NativeWebContentsModalDialog;

namespace {

class NativeWebContentsModalDialogManagerCocoa
    : public web_modal::SingleWebContentsDialogManager {
 public:
  NativeWebContentsModalDialogManagerCocoa() {
  }

  virtual ~NativeWebContentsModalDialogManagerCocoa() {
  }

  // SingleWebContentsDialogManager overrides
  virtual void ManageDialog(NativeWebContentsModalDialog dialog) OVERRIDE {
  }

  virtual void ShowDialog(NativeWebContentsModalDialog dialog) OVERRIDE {
    GetConstrainedWindowMac(dialog)->ShowWebContentsModalDialog();
  }

  virtual void HideDialog(NativeWebContentsModalDialog dialog) OVERRIDE {
  }

  virtual void CloseDialog(NativeWebContentsModalDialog dialog) OVERRIDE {
    GetConstrainedWindowMac(dialog)->CloseWebContentsModalDialog();
  }

  virtual void FocusDialog(NativeWebContentsModalDialog dialog) OVERRIDE {
    GetConstrainedWindowMac(dialog)->FocusWebContentsModalDialog();
  }

  virtual void PulseDialog(NativeWebContentsModalDialog dialog) OVERRIDE {
    GetConstrainedWindowMac(dialog)->PulseWebContentsModalDialog();
  }

  virtual void HostChanged(
      web_modal::WebContentsModalDialogHost* new_host) OVERRIDE {
  }

 private:
  static ConstrainedWindowMac* GetConstrainedWindowMac(
      NativeWebContentsModalDialog dialog) {
    return static_cast<ConstrainedWindowMac*>(dialog);
  }

  DISALLOW_COPY_AND_ASSIGN(NativeWebContentsModalDialogManagerCocoa);
};

}  // namespace

namespace web_modal {

SingleWebContentsDialogManager*
    WebContentsModalDialogManager::CreateNativeWebModalManager(
        SingleWebContentsDialogManagerDelegate* native_delegate) {
  return new NativeWebContentsModalDialogManagerCocoa;
}

}  // namespace web_modal
