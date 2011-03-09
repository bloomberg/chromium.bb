// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_modal_dialogs/js_modal_dialog.h"

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/ui/app_modal_dialogs/native_app_modal_dialog.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_service.h"
#include "content/common/notification_type.h"
#include "ui/base/text/text_elider.h"

namespace {

// The maximum sizes of various texts passed to us from javascript.
const int kMessageTextMaxRows = 32;
const int kMessageTextMaxCols = 132;
const int kDefaultPromptTextSize = 2000;

}  // namespace

JavaScriptAppModalDialog::JavaScriptAppModalDialog(
    JavaScriptAppModalDialogDelegate* delegate,
    const std::wstring& title,
    int dialog_flags,
    const std::wstring& message_text,
    const std::wstring& default_prompt_text,
    bool display_suppress_checkbox,
    bool is_before_unload_dialog,
    IPC::Message* reply_msg)
    : AppModalDialog(delegate->AsTabContents(), title),
      delegate_(delegate),
      extension_host_(delegate->AsExtensionHost()),
      dialog_flags_(dialog_flags),
      display_suppress_checkbox_(display_suppress_checkbox),
      is_before_unload_dialog_(is_before_unload_dialog),
      reply_msg_(reply_msg) {
  // We trim the various parts of the message dialog because otherwise we can
  // overflow the message dialog (and crash/hang the GTK+ version).
  string16 elided_text;
  ui::ElideRectangleString(WideToUTF16(message_text),
      kMessageTextMaxRows, kMessageTextMaxCols, &elided_text);
  message_text_ = UTF16ToWide(elided_text);
  ui::ElideString(WideToUTF16Hack(default_prompt_text), kDefaultPromptTextSize,
                  &default_prompt_text_);

  DCHECK((tab_contents_ != NULL) != (extension_host_ != NULL));
  InitNotifications();
}

JavaScriptAppModalDialog::~JavaScriptAppModalDialog() {
}

NativeAppModalDialog* JavaScriptAppModalDialog::CreateNativeDialog() {
  gfx::NativeWindow parent_window = tab_contents_ ?
      tab_contents_->GetMessageBoxRootWindow() :
      extension_host_->GetMessageBoxRootWindow();
  return NativeAppModalDialog::CreateNativeJavaScriptPrompt(this,
                                                            parent_window);
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
  // Also clear the delegate, since it's now invalid.
  skip_this_dialog_ = true;
  delegate_ = NULL;
  if (native_dialog_)
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

void JavaScriptAppModalDialog::OnCancel(bool suppress_js_messages) {
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

  NotifyDelegate(false, L"", suppress_js_messages);
}

void JavaScriptAppModalDialog::OnAccept(const std::wstring& prompt_text,
                                        bool suppress_js_messages) {
  CompleteDialog();
  NotifyDelegate(true, prompt_text, suppress_js_messages);
}

void JavaScriptAppModalDialog::OnClose() {
  NotifyDelegate(false, L"", false);
}

void JavaScriptAppModalDialog::NotifyDelegate(bool success,
                                              const std::wstring& prompt_text,
                                              bool suppress_js_messages) {
  if (skip_this_dialog_)
    return;

  delegate_->OnMessageBoxClosed(reply_msg_, success, prompt_text);
  if (suppress_js_messages)
    delegate_->SetSuppressMessageBoxes(true);

  // On Views, we can end up coming through this code path twice :(.
  // See crbug.com/63732.
  skip_this_dialog_ = true;
}
