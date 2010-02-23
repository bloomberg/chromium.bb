// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/app_modal_dialog_queue.h"

#include "chrome/browser/browser_list.h"

void AppModalDialogQueue::AddDialog(AppModalDialog* dialog) {
  if (!active_dialog_) {
    ShowModalDialog(dialog);
    return;
  }
  app_modal_dialog_queue_.push(dialog);
}

void AppModalDialogQueue::ShowNextDialog() {
  AppModalDialog* dialog = GetNextDialog();
  if (dialog)
    ShowModalDialog(dialog);
  else
    active_dialog_ = NULL;
}

void AppModalDialogQueue::ActivateModalDialog() {
  if (active_dialog_)
    active_dialog_->ActivateModalDialog();
}

void AppModalDialogQueue::ShowModalDialog(AppModalDialog* dialog) {
  // Must happen before |ShowModalDialog()| is called, because
  // |ShowModalDialog()| might end up calling |ShowNextDialog()|.
  active_dialog_ = dialog;
  dialog->ShowModalDialog();
}

AppModalDialog* AppModalDialogQueue::GetNextDialog() {
  while (!app_modal_dialog_queue_.empty()) {
    AppModalDialog* dialog = app_modal_dialog_queue_.front();
    app_modal_dialog_queue_.pop();
    if (dialog->IsValid())
      return dialog;
    delete dialog;
  }
  return NULL;
}
