// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/message_box_handler.h"

#include "app/l10n_util.h"
#include "app/message_box_flags.h"
#include "build/build_config.h"
#include "chrome/browser/app_modal_dialog_queue.h"
#include "chrome/browser/browsing_data_local_storage_helper.h"
#include "chrome/browser/cookie_modal_dialog.h"
#include "chrome/browser/cookie_prompt_modal_dialog_delegate.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/js_modal_dialog.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"

void RunJavascriptMessageBox(JavaScriptMessageBoxClient* client,
                             const GURL& frame_url,
                             int dialog_flags,
                             const std::wstring& message_text,
                             const std::wstring& default_prompt_text,
                             bool display_suppress_checkbox,
                             IPC::Message* reply_msg) {
  std::wstring title = client->GetMessageBoxTitle(frame_url,
      (dialog_flags == MessageBoxFlags::kIsJavascriptAlert));
  Singleton<AppModalDialogQueue>()->AddDialog(new JavaScriptAppModalDialog(
      client, title, dialog_flags, message_text, default_prompt_text,
      display_suppress_checkbox, false, reply_msg));
}

void RunBeforeUnloadDialog(TabContents* tab_contents,
                           const std::wstring& message_text,
                           IPC::Message* reply_msg) {
  std::wstring full_message =
      message_text + L"\n\n" +
      l10n_util::GetString(IDS_BEFOREUNLOAD_MESSAGEBOX_FOOTER);
  Singleton<AppModalDialogQueue>()->AddDialog(new JavaScriptAppModalDialog(
      tab_contents, l10n_util::GetString(IDS_BEFOREUNLOAD_MESSAGEBOX_TITLE),
      MessageBoxFlags::kIsJavascriptConfirm, message_text, std::wstring(),
      false, true, reply_msg));
}

void RunCookiePrompt(TabContents* tab_contents,
                     HostContentSettingsMap* host_content_settings_map,
                     const GURL& origin,
                     const std::string& cookie_line,
                     CookiePromptModalDialogDelegate* delegate) {
  Singleton<AppModalDialogQueue>()->AddDialog(
      new CookiePromptModalDialog(tab_contents, host_content_settings_map,
                                  origin, cookie_line, delegate));
}

void RunLocalStoragePrompt(
    TabContents* tab_contents,
    HostContentSettingsMap* host_content_settings_map,
    const GURL& origin,
    const string16& key,
    const string16& value,
    CookiePromptModalDialogDelegate* delegate) {
  Singleton<AppModalDialogQueue>()->AddDialog(
      new CookiePromptModalDialog(tab_contents, host_content_settings_map,
                                  origin, key, value, delegate));
}

void RunDatabasePrompt(
    TabContents* tab_contents,
    HostContentSettingsMap* host_content_settings_map,
    const GURL& origin,
    const string16& database_name,
    const string16& display_name,
    unsigned long estimated_size,
    CookiePromptModalDialogDelegate* delegate) {
  Singleton<AppModalDialogQueue>()->AddDialog(
      new CookiePromptModalDialog(tab_contents, host_content_settings_map,
                                  origin, database_name, display_name,
                                  estimated_size, delegate));
}

void RunAppCachePrompt(
    TabContents* tab_contents,
    HostContentSettingsMap* host_content_settings_map,
    const GURL& manifest_url,
    CookiePromptModalDialogDelegate* delegate) {
  Singleton<AppModalDialogQueue>()->AddDialog(
      new CookiePromptModalDialog(tab_contents, host_content_settings_map,
                                  manifest_url, delegate));
}
