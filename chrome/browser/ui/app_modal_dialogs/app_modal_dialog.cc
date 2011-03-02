// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_modal_dialogs/app_modal_dialog.h"

#include "chrome/browser/ui/app_modal_dialogs/app_modal_dialog_queue.h"
#include "chrome/browser/ui/app_modal_dialogs/native_app_modal_dialog.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "content/browser/tab_contents/tab_contents.h"

AppModalDialog::AppModalDialog(TabContents* tab_contents,
                               const std::wstring& title)
    : skip_this_dialog_(false),
      tab_contents_(tab_contents),
      native_dialog_(NULL),
      title_(title) {
}

AppModalDialog::~AppModalDialog() {
}

void AppModalDialog::ShowModalDialog() {
  if (tab_contents_)
    tab_contents_->Activate();

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
  return !skip_this_dialog_;
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
