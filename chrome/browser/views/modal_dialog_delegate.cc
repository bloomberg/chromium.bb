// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/modal_dialog_delegate.h"

#include "base/logging.h"
#include "views/window/window.h"

void ModalDialogDelegate::ShowModalDialog() {
  gfx::NativeWindow root_hwnd = GetDialogRootWindow();
  // GetMessageBoxRootWindow() will be NULL if there's no selected tab (e.g.,
  // during shutdown), in which case we simply skip showing this dialog.
  if (!root_hwnd) {
    Cancel();
  } else {
    dialog_ = views::Window::CreateChromeWindow(root_hwnd, gfx::Rect(), this);
    dialog_->Show();
  }
}

void ModalDialogDelegate::ActivateModalDialog() {
  DCHECK(dialog_);
  // Ensure that the dialog is visible and at the top of the z-order. These
  // conditions may not be true if the dialog was opened on a different virtual
  // desktop to the one the browser window is on.
  dialog_->Show();
  dialog_->Activate();
}

void ModalDialogDelegate::CloseModalDialog() {
  // If the dialog is visible close it.
  if (dialog_)
    dialog_->Close();
}

ModalDialogDelegate::ModalDialogDelegate() : dialog_(NULL) {
}

