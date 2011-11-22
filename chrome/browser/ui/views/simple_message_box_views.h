// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
  friend class base::RefCounted<SimpleMessageBoxViews>;

  // The state of the dialog when closing.
  enum DispositionType {
    DISPOSITION_UNKNOWN,
    DISPOSITION_CANCEL,
    DISPOSITION_OK
  };

  // Message box is modal to |parent_window|.
  static void ShowErrorBox(gfx::NativeWindow parent_window,
                           const string16& title,
                           const string16& message);
  static bool ShowYesNoBox(gfx::NativeWindow parent_window,
                           const string16& title,
                           const string16& message);

  // Overridden from views::DialogDelegate:
  virtual bool Cancel() OVERRIDE;
  virtual bool Accept() OVERRIDE;

  // Returns true if the Accept button was clicked.
  const bool Accepted() {
    return disposition_ == DISPOSITION_OK;
  }

 protected:
  // Overridden from views::DialogDelegate:
  virtual int GetDialogButtons() const OVERRIDE;
  virtual string16 GetDialogButtonLabel(ui::DialogButton button) const OVERRIDE;

  // Overridden from views::WidgetDelegate:
  virtual bool ShouldShowWindowTitle() const OVERRIDE;
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;
  virtual bool IsModal() const OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;
  virtual const views::Widget* GetWidget() const OVERRIDE;

 private:
  SimpleMessageBoxViews(gfx::NativeWindow parent_window,
                        int dialog_flags,
                        const string16& title,
                        const string16& message);
  virtual ~SimpleMessageBoxViews();

  // MessageLoop::Dispatcher implementation.
#if defined(OS_WIN)
  // Dispatcher method. This returns true if the menu was canceled, or
  // if the message is such that the menu should be closed.
  virtual bool Dispatch(const MSG& msg) OVERRIDE;
#elif defined(USE_AURA)
  virtual base::MessagePumpDispatcher::DispatchStatus Dispatch(
      XEvent* xevent) OVERRIDE;
#else
  virtual bool Dispatch(GdkEvent* event) OVERRIDE;
#endif

  int dialog_flags_;
  string16 message_box_title_;
  views::MessageBoxView* message_box_view_;
  DispositionType disposition_;

  DISALLOW_COPY_AND_ASSIGN(SimpleMessageBoxViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIMPLE_MESSAGE_BOX_VIEWS_H_
