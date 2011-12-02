// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_JS_MODAL_DIALOG_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_JS_MODAL_DIALOG_VIEWS_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/app_modal_dialogs/native_app_modal_dialog.h"
#include "ui/views/window/dialog_delegate.h"

class JavaScriptAppModalDialog;

namespace views {
class MessageBoxView;
}

class JSModalDialogViews : public NativeAppModalDialog,
                           public views::DialogDelegate {
 public:
  explicit JSModalDialogViews(JavaScriptAppModalDialog* parent);
  virtual ~JSModalDialogViews();

  // Overridden from NativeAppModalDialog:
  virtual int GetAppModalDialogButtons() const OVERRIDE;
  virtual void ShowAppModalDialog() OVERRIDE;
  virtual void ActivateAppModalDialog() OVERRIDE;
  virtual void CloseAppModalDialog() OVERRIDE;
  virtual void AcceptAppModalDialog() OVERRIDE;
  virtual void CancelAppModalDialog() OVERRIDE;

  // Overridden from views::DialogDelegate:
  virtual int GetDefaultDialogButton() const OVERRIDE;
  virtual int GetDialogButtons() const OVERRIDE;
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual void WindowClosing() OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual string16 GetDialogButtonLabel(ui::DialogButton button) const OVERRIDE;

  // Overridden from views::WidgetDelegate:
  virtual bool IsModal() const OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;
  virtual views::View* GetInitiallyFocusedView() OVERRIDE;
  virtual void OnClose() OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;
  virtual const views::Widget* GetWidget() const OVERRIDE;

 private:
  // A pointer to the AppModalDialog that owns us.
  scoped_ptr<JavaScriptAppModalDialog> parent_;

  // The message box view whose commands we handle.
  views::MessageBoxView* message_box_view_;

  DISALLOW_COPY_AND_ASSIGN(JSModalDialogViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_JS_MODAL_DIALOG_VIEWS_H_
