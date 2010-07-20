// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/js_modal_dialog.h"

#include "base/string_util.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "ipc/ipc_message.h"

namespace {

// The maximum sizes of various texts passed to us from javascript.
const int kMessageTextMaxSize = 3000;
const int kDefaultPromptTextSize = 2000;

}  // namespace

JavaScriptAppModalDialog::JavaScriptAppModalDialog(
    JavaScriptMessageBoxClient* client,
    const std::wstring& title,
    int dialog_flags,
    const std::wstring& message_text,
    const std::wstring& default_prompt_text,
    bool display_suppress_checkbox,
    bool is_before_unload_dialog,
    IPC::Message* reply_msg)
    : AppModalDialog(client->AsTabContents(), title),
#if defined(OS_MACOSX)
      dialog_(NULL),
#endif
      client_(client),
      extension_host_(client->AsExtensionHost()),
      dialog_flags_(dialog_flags),
      display_suppress_checkbox_(display_suppress_checkbox),
      is_before_unload_dialog_(is_before_unload_dialog),
      reply_msg_(reply_msg) {
  // We trim the various parts of the message dialog because otherwise we can
  // overflow the message dialog (and crash/hang the GTK+ version).
  ElideString(message_text, kMessageTextMaxSize, &message_text_);
  ElideString(default_prompt_text, kDefaultPromptTextSize,
              &default_prompt_text_);

  DCHECK((tab_contents_ != NULL) != (extension_host_ != NULL));
  InitNotifications();
}

JavaScriptAppModalDialog::~JavaScriptAppModalDialog() {
}

void JavaScriptAppModalDialog::Observe(NotificationType type,
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
  if (dialog_)
    CloseModalDialog();
}

void JavaScriptAppModalDialog::InitNotifications() {
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

void JavaScriptAppModalDialog::OnCancel() {
  // If we are shutting down and this is an onbeforeunload dialog, cancel the
  // shutdown.
  if (is_before_unload_dialog_)
    browser_shutdown::SetTryingToQuit(false);

  // We need to do this before WM_DESTROY (WindowClosing()) as any parent frame
  // will receive its activation messages before this dialog receives
  // WM_DESTROY. The parent frame would then try to activate any modal dialogs
  // that were still open in the ModalDialogQueue, which would send activation
  // back to this one. The framework should be improved to handle this, so this
  // is a temporary workaround.
  CompleteDialog();

  if (!skip_this_dialog_) {
    client_->OnMessageBoxClosed(reply_msg_, false, std::wstring());
  }

  Cleanup();
}

void JavaScriptAppModalDialog::OnAccept(const std::wstring& prompt_text,
                                        bool suppress_js_messages) {
  CompleteDialog();

  if (!skip_this_dialog_) {
    client_->OnMessageBoxClosed(reply_msg_, true, prompt_text);
    if (suppress_js_messages)
      client_->SetSuppressMessageBoxes(true);
  }

  Cleanup();
}

void JavaScriptAppModalDialog::OnClose() {
  Cleanup();
}

void JavaScriptAppModalDialog::Cleanup() {
  if (skip_this_dialog_) {
    // We can't use the client_, because we might be in the process of
    // destroying it.
    if (tab_contents_)
      tab_contents_->OnMessageBoxClosed(reply_msg_, false, L"");
// The extension_host_ will always be a dirty pointer on OS X because the alert
// window will cause the extension popup to close since it is resigning its key
// state, destroying the host. http://crbug.com/29355
#if !defined(OS_MACOSX)
    else if (extension_host_)
      extension_host_->OnMessageBoxClosed(reply_msg_, false, L"");
    else
      NOTREACHED();
#endif
  }
  AppModalDialog::Cleanup();
}
