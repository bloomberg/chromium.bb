// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A dialog box that tells the user that we can't write to the specified user
// data directory.  Provides the user a chance to pick a different directory.

#ifndef CHROME_BROWSER_BLACKLIST_ERROR_DIALOG_H_
#define CHROME_BROWSER_BLACKLIST_ERROR_DIALOG_H_

#include "base/message_loop.h"
#include "views/window/dialog_delegate.h"

class MessageBoxView;

namespace views {
class Window;
}  // namespace views

class BlacklistErrorDialog : public views::DialogDelegate,
                             public MessageLoopForUI::Dispatcher {
 public:
  // Creates and runs a blacklist error dialog which is displayed modally
  // to the user so that they know and acknowledge that their privacy is
  // not protected.
  static bool RunBlacklistErrorDialog();

  virtual ~BlacklistErrorDialog();
  bool accepted() const { return accepted_; }

  // views::DialogDelegate Methods:
  virtual std::wstring GetDialogButtonLabel(
      MessageBoxFlags::DialogButton button) const;
  virtual std::wstring GetWindowTitle() const;
  virtual void DeleteDelegate();
  virtual bool Accept();
  virtual bool Cancel();

  // views::WindowDelegate Methods:
  virtual bool IsAlwaysOnTop() const { return false; }
  virtual bool IsModal() const { return false; }
  virtual views::View* GetContentsView();

  // MessageLoop::Dispatcher Method:
  virtual bool Dispatch(const MSG& msg);

 private:
  BlacklistErrorDialog();

  MessageBoxView* box_view_;

  // Used to keep track of whether or not to block the message loop (still
  // waiting for the user to dismiss the dialog).
  bool is_blocking_;
  bool accepted_;

  DISALLOW_COPY_AND_ASSIGN(BlacklistErrorDialog);
};

#endif // CHROME_BROWSER_BLACKLIST_ERROR_DIALOG_H_
