// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/shell_javascript_dialog_creator.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "content/public/browser/web_contents.h"
#include "content/shell/layout_test_controller_host.h"
#include "content/shell/shell_javascript_dialog.h"
#include "content/shell/shell_switches.h"
#include "net/base/net_util.h"

namespace content {

ShellJavaScriptDialogCreator::ShellJavaScriptDialogCreator() {
}

ShellJavaScriptDialogCreator::~ShellJavaScriptDialogCreator() {
}

void ShellJavaScriptDialogCreator::RunJavaScriptDialog(
    WebContents* web_contents,
    const GURL& origin_url,
    const std::string& accept_lang,
    JavaScriptMessageType javascript_message_type,
    const string16& message_text,
    const string16& default_prompt_text,
    const DialogClosedCallback& callback,
    bool* did_suppress_message) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kDumpRenderTree)) {
    if (javascript_message_type == JAVASCRIPT_MESSAGE_TYPE_ALERT) {
      printf("ALERT: %s\n", UTF16ToUTF8(message_text).c_str());
    } else if (javascript_message_type == JAVASCRIPT_MESSAGE_TYPE_CONFIRM) {
      printf("CONFIRM: %s\n", UTF16ToUTF8(message_text).c_str());
    } else {  // JAVASCRIPT_MESSAGE_TYPE_PROMPT
      printf("PROMPT: %s, default text: %s\n",
             UTF16ToUTF8(message_text).c_str(),
             UTF16ToUTF8(default_prompt_text).c_str());
    }
    callback.Run(true, string16());
    return;
  }

#if defined(OS_MACOSX) || defined(OS_WIN)
  *did_suppress_message = false;

  if (dialog_.get()) {
    // One dialog at a time, please.
    *did_suppress_message = true;
    return;
  }

  string16 new_message_text = net::FormatUrl(origin_url, accept_lang) +
                              ASCIIToUTF16("\n\n") +
                              message_text;

  dialog_.reset(new ShellJavaScriptDialog(this,
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

void ShellJavaScriptDialogCreator::RunBeforeUnloadDialog(
    WebContents* web_contents,
    const string16& message_text,
    bool is_reload,
    const DialogClosedCallback& callback) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kDumpRenderTree)) {
    printf("CONFIRM NAVIGATION: %s\n", UTF16ToUTF8(message_text).c_str());
    LayoutTestControllerHost* controller =
        LayoutTestControllerHost::FromRenderViewHost(
            web_contents->GetRenderViewHost());
    callback.Run(
        !controller->should_stay_on_page_after_handling_before_unload(),
        string16());
    return;
  }

#if defined(OS_MACOSX) || defined(OS_WIN)
  if (dialog_.get()) {
    // Seriously!?
    callback.Run(true, string16());
    return;
  }

  string16 new_message_text =
      message_text +
      ASCIIToUTF16("\n\nIs it OK to leave/reload this page?");

  dialog_.reset(new ShellJavaScriptDialog(this,
                                          JAVASCRIPT_MESSAGE_TYPE_CONFIRM,
                                          new_message_text,
                                          string16(),  // default_prompt_text
                                          callback));
#else
  // TODO: implement ShellJavaScriptDialog for other platforms, drop this #if
  callback.Run(true, string16());
  return;
#endif
}

void ShellJavaScriptDialogCreator::ResetJavaScriptState(
    WebContents* web_contents) {
#if defined(OS_MACOSX) || defined(OS_WIN)
  if (dialog_.get()) {
    dialog_->Cancel();
    dialog_.reset();
  }
#else
  // TODO: implement ShellJavaScriptDialog for other platforms, drop this #if
#endif
}

void ShellJavaScriptDialogCreator::DialogClosed(ShellJavaScriptDialog* dialog) {
#if defined(OS_MACOSX) || defined(OS_WIN)
  DCHECK_EQ(dialog, dialog_.get());
  dialog_.reset();
#else
  // TODO: implement ShellJavaScriptDialog for other platforms, drop this #if
#endif
}

}  // namespace content
