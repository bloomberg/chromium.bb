// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_JAVASCRIPT_DIALOGS_H_
#define CONTENT_BROWSER_JAVASCRIPT_DIALOGS_H_
#pragma once

#include "base/string16.h"
#include "content/common/content_export.h"
#include "ui/gfx/native_widget_types.h"

namespace IPC {
class Message;
}

namespace content {

class CONTENT_EXPORT DialogDelegate {
 public:
  // Returns the root native window with which to associate the dialog.
  virtual gfx::NativeWindow GetDialogRootWindow() const = 0;

  // Called right before the dialog is shown.
  virtual void OnDialogShown() {}

 protected:
  virtual ~DialogDelegate() {}
};

// A class that invokes a JavaScript dialog must implement this interface to
// allow the dialog implementation to get needed information and return results.
class CONTENT_EXPORT JavaScriptDialogDelegate : public DialogDelegate {
 public:
  // This callback is invoked when the dialog is closed.
  virtual void OnDialogClosed(IPC::Message* reply_msg,
                              bool success,
                              const string16& user_input) = 0;

 protected:
  virtual ~JavaScriptDialogDelegate() {}
};

// An interface consisting of methods that can be called to produce JavaScript
// dialogs.
class JavaScriptDialogCreator {
 public:
  enum TitleType {
    DIALOG_TITLE_NONE,
    DIALOG_TITLE_PLAIN_STRING,
    DIALOG_TITLE_FORMATTED_URL
  };

  // Displays a JavaScript dialog. |did_suppress_message| will not be nil; if
  // |true| is returned in it, the caller will handle faking the reply.
  virtual void RunJavaScriptDialog(JavaScriptDialogDelegate* delegate,
                                   TitleType title_type,
                                   const string16& title,
                                   int dialog_flags,
                                   const string16& message_text,
                                   const string16& default_prompt_text,
                                   IPC::Message* reply_message,
                                   bool* did_suppress_message) = 0;

  // Displays a dialog asking the user if they want to leave a page.
  virtual void RunBeforeUnloadDialog(JavaScriptDialogDelegate* delegate,
                                     const string16& message_text,
                                     IPC::Message* reply_message) = 0;

  // Cancels all pending dialogs and resets any saved JavaScript dialog state
  // for the delegate.
  virtual void ResetJavaScriptState(JavaScriptDialogDelegate* delegate) = 0;

 protected:
  virtual ~JavaScriptDialogCreator() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_JAVASCRIPT_DIALOGS_H_
