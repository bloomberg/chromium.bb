// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_SHELL_JAVASCRIPT_DIALOG_H_
#define CONTENT_SHELL_SHELL_JAVASCRIPT_DIALOG_H_
#pragma once

#include "content/public/browser/javascript_dialogs.h"

#if defined(OS_MACOSX)
#if __OBJC__
@class NSAlert;
@class ShellJavaScriptDialogHelper;
#else
class NSAlert;
class ShellJavaScriptDialogHelper;
#endif  // __OBJC__
#endif  // defined(OS_MACOSX)

namespace content {

class ShellJavaScriptDialogCreator;

class ShellJavaScriptDialog {
 public:
  ShellJavaScriptDialog(
      ShellJavaScriptDialogCreator* creator,
      ui::JavascriptMessageType javascript_message_type,
      const string16& message_text,
      const string16& default_prompt_text,
      const JavaScriptDialogCreator::DialogClosedCallback& callback);
  ~ShellJavaScriptDialog();

  // Called to cancel a dialog mid-flight.
  void Cancel();

 private:
  ShellJavaScriptDialogCreator* creator_;
  JavaScriptDialogCreator::DialogClosedCallback callback_;

#if defined(OS_MACOSX)
  ShellJavaScriptDialogHelper* helper_;  // owned
  NSAlert* alert_; // weak, owned by |helper_|.
#endif  // defined(OS_MACOSX)

  DISALLOW_COPY_AND_ASSIGN(ShellJavaScriptDialog);
};

}  // namespace content

#endif  // CONTENT_SHELL_SHELL_JAVASCRIPT_DIALOG_H_
