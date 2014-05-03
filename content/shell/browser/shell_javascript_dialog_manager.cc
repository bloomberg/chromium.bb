// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_javascript_dialog_manager.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/web_contents.h"
#include "content/shell/browser/shell_javascript_dialog.h"
#include "content/shell/browser/webkit_test_controller.h"
#include "content/shell/common/shell_switches.h"
#include "net/base/net_util.h"

namespace content {

ShellJavaScriptDialogManager::ShellJavaScriptDialogManager() {
}

ShellJavaScriptDialogManager::~ShellJavaScriptDialogManager() {
}

void ShellJavaScriptDialogManager::RunJavaScriptDialog(
    WebContents* web_contents,
    const GURL& origin_url,
    const std::string& accept_lang,
    JavaScriptMessageType javascript_message_type,
    const base::string16& message_text,
    const base::string16& default_prompt_text,
    const DialogClosedCallback& callback,
    bool* did_suppress_message) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kDumpRenderTree)) {
    callback.Run(true, base::string16());
    return;
  }

  if (!dialog_request_callback_.is_null()) {
    dialog_request_callback_.Run();
    callback.Run(true, base::string16());
    dialog_request_callback_.Reset();
    return;
  }

#if defined(OS_MACOSX) || defined(OS_WIN)
  *did_suppress_message = false;

  if (dialog_) {
    // One dialog at a time, please.
    *did_suppress_message = true;
    return;
  }

  base::string16 new_message_text = net::FormatUrl(origin_url, accept_lang) +
                              base::ASCIIToUTF16("\n\n") +
                              message_text;
  gfx::NativeWindow parent_window = web_contents->GetTopLevelNativeWindow();

  dialog_.reset(new ShellJavaScriptDialog(this,
                                          parent_window,
                                          javascript_message_type,
                                          new_message_text,
                                          default_prompt_text,
                                          callback));
#else
  // TODO: implement ShellJavaScriptDialog for other platforms, drop this #if
  *did_suppress_message = true;
  return;
#endif
}

void ShellJavaScriptDialogManager::RunBeforeUnloadDialog(
    WebContents* web_contents,
    const base::string16& message_text,
    bool is_reload,
    const DialogClosedCallback& callback) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kDumpRenderTree)) {
    callback.Run(true, base::string16());
    return;
  }

  if (!dialog_request_callback_.is_null()) {
    dialog_request_callback_.Run();
    callback.Run(true, base::string16());
    dialog_request_callback_.Reset();
    return;
  }

#if defined(OS_MACOSX) || defined(OS_WIN)
  if (dialog_) {
    // Seriously!?
    callback.Run(true, base::string16());
    return;
  }

  base::string16 new_message_text =
      message_text +
      base::ASCIIToUTF16("\n\nIs it OK to leave/reload this page?");

  gfx::NativeWindow parent_window = web_contents->GetTopLevelNativeWindow();

  dialog_.reset(new ShellJavaScriptDialog(this,
                                          parent_window,
                                          JAVASCRIPT_MESSAGE_TYPE_CONFIRM,
                                          new_message_text,
                                          base::string16(),  // default
                                          callback));
#else
  // TODO: implement ShellJavaScriptDialog for other platforms, drop this #if
  callback.Run(true, base::string16());
  return;
#endif
}

void ShellJavaScriptDialogManager::CancelActiveAndPendingDialogs(
    WebContents* web_contents) {
#if defined(OS_MACOSX) || defined(OS_WIN)
  if (dialog_) {
    dialog_->Cancel();
    dialog_.reset();
  }
#else
  // TODO: implement ShellJavaScriptDialog for other platforms, drop this #if
#endif
}

void ShellJavaScriptDialogManager::WebContentsDestroyed(
    WebContents* web_contents) {
}

void ShellJavaScriptDialogManager::DialogClosed(ShellJavaScriptDialog* dialog) {
#if defined(OS_MACOSX) || defined(OS_WIN)
  DCHECK_EQ(dialog, dialog_.get());
  dialog_.reset();
#else
  // TODO: implement ShellJavaScriptDialog for other platforms, drop this #if
#endif
}

}  // namespace content
