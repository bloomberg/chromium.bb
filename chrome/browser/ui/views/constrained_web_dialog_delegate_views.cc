// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/constrained_web_dialog_delegate_base.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "chrome/browser/ui/views/unhandled_keyboard_event_handler.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/size.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_ui.h"

using content::WebContents;
using ui::WebDialogDelegate;
using ui::WebDialogWebContentsDelegate;

namespace {

class ConstrainedWebDialogDelegateViews
    : public ConstrainedWebDialogDelegateBase {
 public:
  ConstrainedWebDialogDelegateViews(
      content::BrowserContext* browser_context,
      WebDialogDelegate* delegate,
      WebDialogWebContentsDelegate* tab_delegate,
      views::WebView* view)
      : ConstrainedWebDialogDelegateBase(
            browser_context, delegate, tab_delegate),
        view_(view) {
    WebContents* web_contents = GetWebContents();
    if (tab_delegate) {
      set_override_tab_delegate(tab_delegate);
      web_contents->SetDelegate(tab_delegate);
    } else {
      web_contents->SetDelegate(this);
    }
  }

  virtual ~ConstrainedWebDialogDelegateViews() {}

  // WebDialogWebContentsDelegate interface.
  virtual void CloseContents(WebContents* source) OVERRIDE {
    GetWindow()->CloseWebContentsModalDialog();
  }

  // contents::WebContentsDelegate
  virtual void HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) OVERRIDE {
    unhandled_keyboard_event_handler_.HandleKeyboardEvent(
        event, view_->GetFocusManager());
  }

 private:
  // Converts keyboard events on the WebContents to accelerators.
  UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  views::WebView* view_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedWebDialogDelegateViews);
};

}  // namespace

class ConstrainedWebDialogDelegateViewViews
    : public views::WebView,
      public ConstrainedWebDialogDelegate,
      public views::WidgetDelegate {
 public:
  ConstrainedWebDialogDelegateViewViews(
      content::BrowserContext* browser_context,
      WebDialogDelegate* delegate,
      WebDialogWebContentsDelegate* tab_delegate);
  virtual ~ConstrainedWebDialogDelegateViewViews();

  void set_window(WebContentsModalDialog* window) {
    return impl_->set_window(window);
  }

  // ConstrainedWebDialogDelegate interface
  virtual const WebDialogDelegate*
      GetWebDialogDelegate() const OVERRIDE {
    return impl_->GetWebDialogDelegate();
  }
  virtual WebDialogDelegate* GetWebDialogDelegate() OVERRIDE {
    return impl_->GetWebDialogDelegate();
  }
  virtual void OnDialogCloseFromWebUI() OVERRIDE {
    return impl_->OnDialogCloseFromWebUI();
  }
  virtual void ReleaseWebContentsOnDialogClose() OVERRIDE {
    return impl_->ReleaseWebContentsOnDialogClose();
  }
  virtual WebContentsModalDialog* GetWindow() OVERRIDE {
    return impl_->GetWindow();
  }
  virtual WebContents* GetWebContents() OVERRIDE {
    return impl_->GetWebContents();
  }

  // views::WidgetDelegate interface.
  virtual views::View* GetInitiallyFocusedView() OVERRIDE {
    return this;
  }
  virtual bool CanResize() const OVERRIDE { return true; }
  virtual void WindowClosing() OVERRIDE {
    if (!impl_->closed_via_webui())
      GetWebDialogDelegate()->OnDialogClosed(std::string());
  }
  virtual views::Widget* GetWidget() OVERRIDE {
    return View::GetWidget();
  }
  virtual const views::Widget* GetWidget() const OVERRIDE {
    return View::GetWidget();
  }
  virtual string16 GetWindowTitle() const OVERRIDE {
    return impl_->closed_via_webui() ? string16() :
        GetWebDialogDelegate()->GetDialogTitle();
  }
  virtual views::View* GetContentsView() OVERRIDE {
    return this;
  }

  virtual ui::ModalType GetModalType() const OVERRIDE {
#if defined(USE_ASH)
    return ui::MODAL_TYPE_CHILD;
#else
    return views::WidgetDelegate::GetModalType();
#endif
  }

  virtual void OnWidgetMove() OVERRIDE {
    // We need to check the existence of the widget because when running on
    // WinXP this could get executed before the widget is entirely created.
    if (!GetWidget())
      return;

    GetWidget()->CenterWindow(
        GetWidget()->non_client_view()->GetPreferredSize());
    views::WidgetDelegate::OnWidgetMove();
  }

  // views::WebView overrides.
  virtual bool AcceleratorPressed(
      const ui::Accelerator& accelerator) OVERRIDE {
    // Pressing ESC closes the dialog.
    DCHECK_EQ(ui::VKEY_ESCAPE, accelerator.key_code());
    GetWindow()->CloseWebContentsModalDialog();
    return true;
  }
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    gfx::Size size;
    if (!impl_->closed_via_webui())
      GetWebDialogDelegate()->GetDialogSize(&size);
    return size;
  }
  virtual gfx::Size GetMinimumSize() OVERRIDE {
    // Return an empty size so that we can be made smaller.
    return gfx::Size();
  }

 private:
  scoped_ptr<ConstrainedWebDialogDelegateViews> impl_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedWebDialogDelegateViewViews);
};

ConstrainedWebDialogDelegateViewViews::ConstrainedWebDialogDelegateViewViews(
    content::BrowserContext* browser_context,
    WebDialogDelegate* delegate,
    WebDialogWebContentsDelegate* tab_delegate)
    : views::WebView(browser_context),
      impl_(new ConstrainedWebDialogDelegateViews(browser_context,
                                                  delegate,
                                                  tab_delegate,
                                                  this)) {
  SetWebContents(GetWebContents());

  // Pressing ESC closes the dialog.
  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
}

ConstrainedWebDialogDelegateViewViews::~ConstrainedWebDialogDelegateViewViews() {
}

ConstrainedWebDialogDelegate* CreateConstrainedWebDialog(
    content::BrowserContext* browser_context,
    WebDialogDelegate* delegate,
    WebDialogWebContentsDelegate* tab_delegate,
    content::WebContents* web_contents) {
  ConstrainedWebDialogDelegateViewViews* constrained_delegate =
      new ConstrainedWebDialogDelegateViewViews(
          browser_context, delegate, tab_delegate);
  WebContentsModalDialog* web_contents_modal_dialog =
      new ConstrainedWindowViews(web_contents, constrained_delegate);
  constrained_delegate->set_window(web_contents_modal_dialog);
  return constrained_delegate;
}
