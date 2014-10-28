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
  ~JavaScriptAppModalDialogViews() override;

  // Overridden from NativeAppModalDialog:
  int GetAppModalDialogButtons() const override;
  void ShowAppModalDialog() override;
  void ActivateAppModalDialog() override;
  void CloseAppModalDialog() override;
  void AcceptAppModalDialog() override;
  void CancelAppModalDialog() override;

  // Overridden from views::DialogDelegate:
  int GetDefaultDialogButton() const override;
  int GetDialogButtons() const override;
  base::string16 GetWindowTitle() const override;
  void WindowClosing() override;
  void DeleteDelegate() override;
  bool Cancel() override;
  bool Accept() override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;

  // Overridden from views::WidgetDelegate:
  ui::ModalType GetModalType() const override;
  views::View* GetContentsView() override;
  views::View* GetInitiallyFocusedView() override;
  void OnClosed() override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;

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
