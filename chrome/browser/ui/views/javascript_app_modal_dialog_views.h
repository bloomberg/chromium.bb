// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_JAVASCRIPT_APP_MODAL_DIALOG_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_JAVASCRIPT_APP_MODAL_DIALOG_VIEWS_H_

#include "base/memory/scoped_ptr.h"
#include "components/app_modal_dialogs/native_app_modal_dialog.h"
#include "ui/views/window/dialog_delegate.h"

class JavaScriptAppModalDialog;

#if defined(USE_X11) && !defined(OS_CHROMEOS)
class JavascriptAppModalEventBlockerX11;
#endif

namespace views {
class MessageBoxView;
}

class JavaScriptAppModalDialogViews : public NativeAppModalDialog,
                                      public views::DialogDelegate {
 public:
  explicit JavaScriptAppModalDialogViews(JavaScriptAppModalDialog* parent);
  virtual ~JavaScriptAppModalDialogViews();

  // Overridden from NativeAppModalDialog:
  virtual int GetAppModalDialogButtons() const override;
  virtual void ShowAppModalDialog() override;
  virtual void ActivateAppModalDialog() override;
  virtual void CloseAppModalDialog() override;
  virtual void AcceptAppModalDialog() override;
  virtual void CancelAppModalDialog() override;

  // Overridden from views::DialogDelegate:
  virtual int GetDefaultDialogButton() const override;
  virtual int GetDialogButtons() const override;
  virtual base::string16 GetWindowTitle() const override;
  virtual void WindowClosing() override;
  virtual void DeleteDelegate() override;
  virtual bool Cancel() override;
  virtual bool Accept() override;
  virtual base::string16 GetDialogButtonLabel(
      ui::DialogButton button) const override;

  // Overridden from views::WidgetDelegate:
  virtual ui::ModalType GetModalType() const override;
  virtual views::View* GetContentsView() override;
  virtual views::View* GetInitiallyFocusedView() override;
  virtual void OnClosed() override;
  virtual views::Widget* GetWidget() override;
  virtual const views::Widget* GetWidget() const override;

 private:
  // A pointer to the AppModalDialog that owns us.
  scoped_ptr<JavaScriptAppModalDialog> parent_;

  // The message box view whose commands we handle.
  views::MessageBoxView* message_box_view_;

#if defined(USE_X11) && !defined(OS_CHROMEOS)
  // Blocks events to other browser windows while the dialog is open.
  scoped_ptr<JavascriptAppModalEventBlockerX11> event_blocker_x11_;
#endif

  DISALLOW_COPY_AND_ASSIGN(JavaScriptAppModalDialogViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_JAVASCRIPT_APP_MODAL_DIALOG_VIEWS_H_
