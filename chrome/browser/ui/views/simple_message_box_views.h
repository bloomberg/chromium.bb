// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIMPLE_MESSAGE_BOX_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_SIMPLE_MESSAGE_BOX_VIEWS_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class MessageBoxView;
}

// Simple message box implemented in Views.
class SimpleMessageBoxViews : public views::DialogDelegate,
                              public MessageLoop::Dispatcher,
                              public base::RefCounted<SimpleMessageBoxViews> {
 public:
  static void ShowWarningMessageBox(gfx::NativeWindow parent_window,
                                    const string16& title,
                                    const string16& message);
  static bool ShowQuestionMessageBox(gfx::NativeWindow parent_window,
                                     const string16& title,
                                     const string16& message);

  // Returns true if the Accept button was clicked.
  bool accepted() const { return disposition_ == DISPOSITION_OK; }

 private:
  friend class base::RefCounted<SimpleMessageBoxViews>;

  // The state of the dialog when closing.
  enum DispositionType {
    DISPOSITION_UNKNOWN,
    DISPOSITION_CANCEL,
    DISPOSITION_OK
  };

  enum DialogType {
    DIALOG_TYPE_WARNING,
    DIALOG_TYPE_QUESTION,
  };

  // Overridden from views::DialogDelegate:
  virtual int GetDialogButtons() const OVERRIDE;
  virtual string16 GetDialogButtonLabel(ui::DialogButton button) const OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual bool Accept() OVERRIDE;

  // Overridden from views::WidgetDelegate:
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;
  virtual ui::ModalType GetModalType() const OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;
  virtual const views::Widget* GetWidget() const OVERRIDE;

  SimpleMessageBoxViews(gfx::NativeWindow parent_window,
                        DialogType dialog_type,
                        const string16& title,
                        const string16& message);
  virtual ~SimpleMessageBoxViews();

  // MessageLoop::Dispatcher implementation.
  // Dispatcher method. This returns true if the menu was canceled, or
  // if the message is such that the menu should be closed.
  virtual bool Dispatch(const base::NativeEvent& event) OVERRIDE;

  const DialogType dialog_type_;
  DispositionType disposition_;
  const string16 window_title_;
  views::MessageBoxView* message_box_view_;

  DISALLOW_COPY_AND_ASSIGN(SimpleMessageBoxViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIMPLE_MESSAGE_BOX_VIEWS_H_
