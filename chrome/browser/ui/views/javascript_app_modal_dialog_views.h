// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_JAVASCRIPT_APP_MODAL_DIALOG_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_JAVASCRIPT_APP_MODAL_DIALOG_VIEWS_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/app_modal_dialogs/native_app_modal_dialog.h"
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
  virtual int GetAppModalDialogButtons() const OVERRIDE;
  virtual void ShowAppModalDialog() OVERRIDE;
  virtual void ActivateAppModalDialog() OVERRIDE;
  virtual void CloseAppModalDialog() OVERRIDE;
  virtual void AcceptAppModalDialog() OVERRIDE;
  virtual void CancelAppModalDialog() OVERRIDE;

  // Overridden from views::DialogDelegate:
  virtual int GetDefaultDialogButton() const OVERRIDE;
  virtual int GetDialogButtons() const OVERRIDE;
  virtual base::string16 GetWindowTitle() const OVERRIDE;
  virtual void WindowClosing() OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual base::string16 GetDialogButtonLabel(
      ui::DialogButton button) const OVERRIDE;

  // Overridden from views::WidgetDelegate:
  virtual ui::ModalType GetModalType() const OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;
  virtual views::View* GetInitiallyFocusedView() OVERRIDE;
  virtual void OnClosed() OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;
  virtual const views::Widget* GetWidget() const OVERRIDE;

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
