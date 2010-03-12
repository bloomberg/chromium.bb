// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/app_modal_dialog.h"

#include "chrome/browser/app_modal_dialog_queue.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"

AppModalDialog::AppModalDialog(TabContents* tab_contents,
                               const std::wstring& title)
#if defined(OS_WIN) || defined(OS_LINUX)
    : dialog_(NULL),
#elif defined(OS_MACOSX)
    :
#endif
      tab_contents_(tab_contents),
      title_(title),
      skip_this_dialog_(false) {
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

void AppModalDialog::Cleanup() {
  NotificationService::current()->Notify(
      NotificationType::APP_MODAL_DIALOG_CLOSED,
      Source<AppModalDialog>(this),
      NotificationService::NoDetails());
}

void AppModalDialog::CompleteDialog() {
  Singleton<AppModalDialogQueue>()->ShowNextDialog();
}

