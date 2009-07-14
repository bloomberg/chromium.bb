// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_CONFIRM_MESSAGE_BOX_DIALOG_H_
#define CHROME_BROWSER_VIEWS_CONFIRM_MESSAGE_BOX_DIALOG_H_

#include <string>

#include "base/basictypes.h"
#include "base/gfx/native_widget_types.h"
#include "base/message_loop.h"
#include "views/window/dialog_delegate.h"

class MessageBoxView;

class ConfirmMessageBoxDialog : public views::DialogDelegate,
                                public MessageLoopForUI::Dispatcher {
 public:
  // The method blocks while the dialog is showing, and returns the the value
  // of the user choice, if the user click in Yes button it returns true,
  // otherwise false
  static bool Run(gfx::NativeWindow parent,
                  const std::wstring& message_text,
                  const std::wstring& window_title);

  virtual ~ConfirmMessageBoxDialog();

  bool accepted() const { return accepted_; }

  // views::DialogDelegate implementation.
  virtual int GetDialogButtons() const;
  virtual std::wstring GetWindowTitle() const;
  virtual std::wstring GetDialogButtonLabel(
      MessageBoxFlags::DialogButton button) const;
  virtual bool Accept();
  virtual bool Cancel();

  // views::WindowDelegate  implementation.
  virtual bool IsModal() const { return true; }
  virtual views::View* GetContentsView();
  virtual void DeleteDelegate();

  // MessageLoop::Dispatcher implementation.
  virtual bool Dispatch(const MSG& msg);

 private:
  ConfirmMessageBoxDialog(gfx::NativeWindow parent,
                          const std::wstring& message_text,
                          const std::wstring& window_title);

  // The message which will be shown to user.
  std::wstring message_text_;

  // This is the Title bar text.
  std::wstring window_title_;

  MessageBoxView* message_box_view_;

  // Returns true if the user clicks in Yes button, otherwise false
  bool accepted_;

  // Used to keep track of whether or not to block the message loop (still
  // waiting for the user to dismiss the dialog).
  bool is_blocking_;

  DISALLOW_COPY_AND_ASSIGN(ConfirmMessageBoxDialog);
};

#endif  // CHROME_BROWSER_VIEWS_CONFIRM_MESSAGE_BOX_DIALOG_H_
