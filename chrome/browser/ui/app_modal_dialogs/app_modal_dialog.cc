// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_modal_dialogs/app_modal_dialog.h"

#include "chrome/browser/ui/app_modal_dialogs/app_modal_dialog_queue.h"
#include "chrome/browser/ui/app_modal_dialogs/native_app_modal_dialog.h"
#include "content/browser/javascript_dialogs.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_service.h"
#include "content/common/notification_type.h"

AppModalDialog::AppModalDialog(content::DialogDelegate* delegate,
                               const string16& title)
    : valid_(true),
      delegate_(delegate),
      native_dialog_(NULL),
      title_(title) {
}

AppModalDialog::~AppModalDialog() {
}

void AppModalDialog::ShowModalDialog() {
  if (delegate_)
    delegate_->OnDialogShown();

  CreateAndShowDialog();

  NotificationService::current()->Notify(
      NotificationType::APP_MODAL_DIALOG_SHOWN,
      Source<AppModalDialog>(this),
      NotificationService::NoDetails());
}

void AppModalDialog::CreateAndShowDialog() {
  native_dialog_ = CreateNativeDialog();
  native_dialog_->ShowAppModalDialog();
}

bool AppModalDialog::IsValid() {
  return valid_;
}

void AppModalDialog::Invalidate() {
  valid_ = false;
}

bool AppModalDialog::IsJavaScriptModalDialog() {
  return false;
}

content::DialogDelegate* AppModalDialog::delegate() const {
  return delegate_;
}

void AppModalDialog::ActivateModalDialog() {
  DCHECK(native_dialog_);
  native_dialog_->ActivateAppModalDialog();
}

void AppModalDialog::CloseModalDialog() {
  DCHECK(native_dialog_);
  native_dialog_->CloseAppModalDialog();
}

void AppModalDialog::CompleteDialog() {
  AppModalDialogQueue::GetInstance()->ShowNextDialog();
}
