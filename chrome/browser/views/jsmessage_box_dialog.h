// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_JSMESSAGE_BOX_DIALOG_H_
#define CHROME_BROWSER_VIEWS_JSMESSAGE_BOX_DIALOG_H_
#pragma once

#include "chrome/browser/js_modal_dialog.h"

#include <string>

#include "app/message_box_flags.h"
#include "chrome/browser/jsmessage_box_client.h"
#include "chrome/browser/native_app_modal_dialog.h"
#include "views/window/dialog_delegate.h"

class MessageBoxView;

class JavaScriptMessageBoxDialog : public NativeAppModalDialog,
                                   public views::DialogDelegate {
 public:
  explicit JavaScriptMessageBoxDialog(JavaScriptAppModalDialog* parent);
  virtual ~JavaScriptMessageBoxDialog();

  // Overridden from NativeAppModalDialog:
  virtual int GetAppModalDialogButtons() const;
  virtual void ShowAppModalDialog();
  virtual void ActivateAppModalDialog();
  virtual void CloseAppModalDialog();
  virtual void AcceptAppModalDialog();
  virtual void CancelAppModalDialog();

  // Overridden from views::DialogDelegate:
  virtual int GetDefaultDialogButton() const;
  virtual int GetDialogButtons() const;
  virtual std::wstring GetWindowTitle() const;
  virtual void WindowClosing();
  virtual void DeleteDelegate();
  virtual bool Cancel();
  virtual bool Accept();
  virtual std::wstring GetDialogButtonLabel(
      MessageBoxFlags::DialogButton button) const;

  // Overridden from views::WindowDelegate:
  virtual bool IsModal() const { return true; }
  virtual views::View* GetContentsView();
  virtual views::View* GetInitiallyFocusedView();
  virtual void OnClose();

 private:
  JavaScriptMessageBoxClient* client() const { return parent_->client(); }

  // A pointer to the AppModalDialog that owns us.
  JavaScriptAppModalDialog* parent_;

  // The message box view whose commands we handle.
  MessageBoxView* message_box_view_;

  DISALLOW_COPY_AND_ASSIGN(JavaScriptMessageBoxDialog);
};

#endif  // CHROME_BROWSER_VIEWS_JSMESSAGE_BOX_DIALOG_H_
