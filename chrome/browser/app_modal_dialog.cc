// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/app_modal_dialog.h"

#include "chrome/browser/app_modal_dialog_queue.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "ipc/ipc_message.h"

AppModalDialog::AppModalDialog(JavaScriptMessageBoxClient* client,
                               const std::wstring& title,
                               int dialog_flags,
                               const std::wstring& message_text,
                               const std::wstring& default_prompt_text,
                               bool display_suppress_checkbox,
                               bool is_before_unload_dialog,
                               IPC::Message* reply_msg)
    : dialog_(NULL),
      client_(client),
      tab_contents_(client_->AsTabContents()),
      extension_host_(client_->AsExtensionHost()),
      skip_this_dialog_(false),
      title_(title),
      dialog_flags_(dialog_flags),
      message_text_(message_text),
      default_prompt_text_(default_prompt_text),
      display_suppress_checkbox_(display_suppress_checkbox),
      is_before_unload_dialog_(is_before_unload_dialog),
      reply_msg_(reply_msg) {
  InitNotifications();
  DCHECK((tab_contents_ != NULL) != (extension_host_ != NULL));
}

void AppModalDialog::Observe(NotificationType type,
                             const NotificationSource& source,
                             const NotificationDetails& details) {
  if (skip_this_dialog_)
    return;

  if (NotificationType::EXTENSION_HOST_DESTROYED == type &&
      Details<ExtensionHost>(extension_host_) != details)
    return;

  // If we reach here, we know the notification is relevant to us, either
  // because we're only observing applicable sources or because we passed the
  // check above. Both of those indicate that we should ignore this dialog.
  // Also clear the client, since it's now invalid.
  skip_this_dialog_ = true;
  client_ = NULL;
  CloseModalDialog();
}

void AppModalDialog::InitNotifications() {
  // Make sure we get relevant navigation notifications so we know when our
  // parent contents will disappear or navigate to a different page.
  if (tab_contents_) {
    registrar_.Add(this, NotificationType::NAV_ENTRY_COMMITTED,
                   Source<NavigationController>(&tab_contents_->controller()));
    registrar_.Add(this, NotificationType::TAB_CONTENTS_DESTROYED,
                   Source<TabContents>(tab_contents_));
  } else if (extension_host_) {
    // EXTENSION_HOST_DESTROYED uses the Profile as its source, but we care
    // about the ExtensionHost (which is passed in the details).
    registrar_.Add(this, NotificationType::EXTENSION_HOST_DESTROYED,
                   NotificationService::AllSources());
  } else {
    NOTREACHED();
  }
}

void AppModalDialog::ShowModalDialog() {
  // If the TabContents or ExtensionHost that created this dialog navigated
  // away or was destroyed before this dialog became visible, simply show the
  // next dialog if any.
  if (skip_this_dialog_) {
    Singleton<AppModalDialogQueue>()->ShowNextDialog();
    delete this;
    return;
  }
  if (tab_contents_)
    tab_contents_->Activate();

  CreateAndShowDialog();

  NotificationService::current()->Notify(
      NotificationType::APP_MODAL_DIALOG_SHOWN,
      Source<AppModalDialog>(this),
      NotificationService::NoDetails());
}

void AppModalDialog::OnCancel() {
  // We need to do this before WM_DESTROY (WindowClosing()) as any parent frame
  // will receive its activation messages before this dialog receives
  // WM_DESTROY. The parent frame would then try to activate any modal dialogs
  // that were still open in the ModalDialogQueue, which would send activation
  // back to this one. The framework should be improved to handle this, so this
  // is a temporary workaround.
  Singleton<AppModalDialogQueue>()->ShowNextDialog();

  if (!skip_this_dialog_) {
    client_->OnMessageBoxClosed(reply_msg_, false, std::wstring());
  }

  Cleanup();
}

void AppModalDialog::OnAccept(const std::wstring& prompt_text,
                              bool suppress_js_messages) {
  Singleton<AppModalDialogQueue>()->ShowNextDialog();

  if (!skip_this_dialog_) {
    client_->OnMessageBoxClosed(reply_msg_, true, prompt_text);
    if (suppress_js_messages)
      client_->SetSuppressMessageBoxes(true);
  }

  Cleanup();
}

void AppModalDialog::OnClose() {
  Cleanup();
}

void AppModalDialog::Cleanup() {
  if (skip_this_dialog_) {
    // We can't use the client_, because we might be in the process of
    // destroying it.
    if (tab_contents_)
      tab_contents_->OnMessageBoxClosed(reply_msg_, false, L"");
    else if (extension_host_)
      extension_host_->OnMessageBoxClosed(reply_msg_, false, L"");
    else
      NOTREACHED();
  }
  NotificationService::current()->Notify(
    NotificationType::APP_MODAL_DIALOG_CLOSED,
    Source<AppModalDialog>(this),
    NotificationService::NoDetails());
}
