// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_contents_modal_dialog_manager.h"

#include "chrome/browser/ui/cocoa/constrained_window/constrained_window_mac.h"
#include "chrome/browser/ui/native_web_contents_modal_dialog_manager.h"

namespace {

class NativeWebContentsModalDialogManagerCocoa
    : public NativeWebContentsModalDialogManager {
 public:
  NativeWebContentsModalDialogManagerCocoa() {
  }

  virtual ~NativeWebContentsModalDialogManagerCocoa() {
  }

  // NativeWebContentsModalDialogManager overrides
  virtual void ManageDialog(NativeWebContentsModalDialog dialog) OVERRIDE {
  }

  virtual void ShowDialog(NativeWebContentsModalDialog dialog) OVERRIDE {
    GetConstrainedWindowMac(dialog)->ShowWebContentsModalDialog();
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

 private:
  static ConstrainedWindowMac* GetConstrainedWindowMac(
      NativeWebContentsModalDialog dialog) {
    return static_cast<ConstrainedWindowMac*>(dialog);
  }

  DISALLOW_COPY_AND_ASSIGN(NativeWebContentsModalDialogManagerCocoa);
};

}  // namespace

NativeWebContentsModalDialogManager*
    WebContentsModalDialogManager::CreateNativeManager(
        NativeWebContentsModalDialogManagerDelegate* native_delegate) {
  return new NativeWebContentsModalDialogManagerCocoa;
}
