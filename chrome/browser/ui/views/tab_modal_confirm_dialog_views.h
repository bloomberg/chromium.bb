// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_MODAL_CONFIRM_DIALOG_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_MODAL_CONFIRM_DIALOG_VIEWS_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/window/dialog_delegate.h"

namespace content {
class WebContents;
class BrowserContext;
}

namespace views {
class MessageBoxView;
class Widget;
}

// Displays a tab-modal dialog, i.e. a dialog that will block the current page
// but still allow the user to switch to a different page.
// To display the dialog, allocate this object on the heap. It will open the
// dialog from its constructor and then delete itself when the user dismisses
// the dialog.
class TabModalConfirmDialogViews : public TabModalConfirmDialog,
                                   public views::DialogDelegate {
 public:
  TabModalConfirmDialogViews(TabModalConfirmDialogDelegate* delegate,
                             content::WebContents* web_contents);

  // views::DialogDelegate:
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual string16 GetDialogButtonLabel(ui::DialogButton button) const OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual bool Accept() OVERRIDE;

  // views::WidgetDelegate:
  virtual views::View* GetContentsView() OVERRIDE;
  virtual views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;
  virtual const views::Widget* GetWidget() const OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;
  virtual ui::ModalType GetModalType() const OVERRIDE;

 private:
  virtual ~TabModalConfirmDialogViews();

  // TabModalConfirmDialog:
  virtual void AcceptTabModalDialog() OVERRIDE;
  virtual void CancelTabModalDialog() OVERRIDE;

  // TabModalConfirmDialogCloseDelegate:
  virtual void CloseDialog() OVERRIDE;

  scoped_ptr<TabModalConfirmDialogDelegate> delegate_;

  // The message box view whose commands we handle.
  views::MessageBoxView* message_box_view_;

  views::Widget* dialog_;
  content::BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(TabModalConfirmDialogViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_MODAL_CONFIRM_DIALOG_VIEWS_H_
