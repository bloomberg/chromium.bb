// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONFIRM_MESSAGE_BOX_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_CONFIRM_MESSAGE_BOX_DIALOG_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "ui/gfx/native_widget_types.h"
#include "views/controls/label.h"
#include "views/window/dialog_delegate.h"

// An interface the confirm dialog uses to notify its clients (observers) when
// the user makes a decision to confirm or cancel.  Only one method will be
// invoked per use (i.e per invocation of ConfirmMessageBoxDialog::Run).
class ConfirmMessageBoxObserver {
 public:
  // The user explicitly confirmed by clicking "OK".
  virtual void OnConfirmMessageAccept() = 0;
  // The user chose not to confirm either by clicking "Cancel" or by closing
  // the dialog.
  virtual void OnConfirmMessageCancel() {}
};

class ConfirmMessageBoxDialog : public views::DialogDelegate,
                                public views::View {
 public:
  // The method presents a modal confirmation dialog to the user with the title
  // |window_title| and message |message_text|, and 'Yes' 'No' buttons.
  // |observer| will be notified when the user makes a decision or closes the
  // dialog. Note that this class guarantees it will call one of the observer's
  // methods, so it is the caller's responsibility to ensure |observer| lives
  // until one of the methods is invoked; it can be deleted thereafter from this
  // class' point of view. |parent| specifies where to insert the view into the
  // hierarchy and effectively assumes ownership of the dialog.
  static void Run(gfx::NativeWindow parent,
                  ConfirmMessageBoxObserver* observer,
                  const std::wstring& message_text,
                  const std::wstring& window_title);

  // A variant of the above for when the message text is longer/shorter than
  // what the default size of this dialog can accommodate.
  static void RunWithCustomConfiguration(gfx::NativeWindow parent,
                                         ConfirmMessageBoxObserver* observer,
                                         const std::wstring& message_text,
                                         const std::wstring& window_title,
                                         const std::wstring& confirm_label,
                                         const std::wstring& reject_label,
                                         const gfx::Size& preferred_size);

  virtual ~ConfirmMessageBoxDialog() {}

  // views::DialogDelegate implementation.
  virtual int GetDialogButtons() const;
  virtual std::wstring GetWindowTitle() const;
  virtual std::wstring GetDialogButtonLabel(
      MessageBoxFlags::DialogButton button) const;
  virtual int GetDefaultDialogButton() const {
    return MessageBoxFlags::DIALOGBUTTON_CANCEL;
  }

  virtual bool Accept();
  virtual bool Cancel();

  // views::WindowDelegate  implementation.
  virtual bool IsModal() const { return true; }
  virtual views::View* GetContentsView() { return this; }

  // views::View implementation.
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();

 private:
  ConfirmMessageBoxDialog(ConfirmMessageBoxObserver* observer,
                          const std::wstring& message_text,
                          const std::wstring& window_title);

  // The message which will be shown to user.
  views::Label* message_label_;

  // This is the Title bar text.
  std::wstring window_title_;

  // The text for the 'OK' and 'CANCEL' buttons.
  std::wstring confirm_label_;
  std::wstring reject_label_;

  // The preferred size of the dialog.
  gfx::Size preferred_size_;

  // The observer to notify of acceptance or cancellation.
  ConfirmMessageBoxObserver* observer_;

  DISALLOW_COPY_AND_ASSIGN(ConfirmMessageBoxDialog);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CONFIRM_MESSAGE_BOX_DIALOG_H_
