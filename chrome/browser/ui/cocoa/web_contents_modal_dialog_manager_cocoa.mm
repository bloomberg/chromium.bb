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
  NativeWebContentsModalDialogManagerCocoa(
      NativeWebContentsModalDialog dialog)
      : dialog_(dialog) {
  }

  virtual ~NativeWebContentsModalDialogManagerCocoa() {
  }

  // SingleWebContentsDialogManager overrides
  virtual void Show() OVERRIDE {
    GetConstrainedWindowMac(dialog())->ShowWebContentsModalDialog();
  }

  virtual void Hide() OVERRIDE {
  }

  virtual void Close() OVERRIDE {
    GetConstrainedWindowMac(dialog())->CloseWebContentsModalDialog();
  }

  virtual void Focus() OVERRIDE {
    GetConstrainedWindowMac(dialog())->FocusWebContentsModalDialog();
  }

  virtual void Pulse() OVERRIDE {
    GetConstrainedWindowMac(dialog())->PulseWebContentsModalDialog();
  }

  virtual void HostChanged(
      web_modal::WebContentsModalDialogHost* new_host) OVERRIDE {
  }

  virtual NativeWebContentsModalDialog dialog() OVERRIDE {
    return dialog_;
  }

 private:
  static ConstrainedWindowMac* GetConstrainedWindowMac(
      NativeWebContentsModalDialog dialog) {
    return static_cast<ConstrainedWindowMac*>(dialog);
  }

  // In mac this is a pointer to a ConstrainedWindowMac.
  // TODO(gbillock): Replace this casting system with a more typesafe call path.
  NativeWebContentsModalDialog dialog_;

  DISALLOW_COPY_AND_ASSIGN(NativeWebContentsModalDialogManagerCocoa);
};

}  // namespace

namespace web_modal {

SingleWebContentsDialogManager*
    WebContentsModalDialogManager::CreateNativeWebModalManager(
        NativeWebContentsModalDialog dialog,
        SingleWebContentsDialogManagerDelegate* native_delegate) {
  return new NativeWebContentsModalDialogManagerCocoa(dialog);
}

}  // namespace web_modal
