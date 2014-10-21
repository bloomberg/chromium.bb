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

  ~NativeWebContentsModalDialogManagerCocoa() override {}

  // SingleWebContentsDialogManager overrides
  void Show() override {
    GetConstrainedWindowMac(dialog())->ShowWebContentsModalDialog();
  }

  void Hide() override {}

  void Close() override {
    GetConstrainedWindowMac(dialog())->CloseWebContentsModalDialog();
  }

  void Focus() override {
    GetConstrainedWindowMac(dialog())->FocusWebContentsModalDialog();
  }

  void Pulse() override {
    GetConstrainedWindowMac(dialog())->PulseWebContentsModalDialog();
  }

  void HostChanged(web_modal::WebContentsModalDialogHost* new_host) override {}

  NativeWebContentsModalDialog dialog() override { return dialog_; }

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
