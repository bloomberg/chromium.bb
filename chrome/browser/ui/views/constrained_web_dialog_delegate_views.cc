// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/constrained_web_dialog_delegate_base.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_ui.h"

#if defined(USE_AURA)
#include "content/public/browser/render_widget_host_view.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/client/focus_client.h"
#endif

using ui::WebDialogDelegate;
using ui::WebDialogWebContentsDelegate;

namespace {

class ConstrainedWebDialogDelegateViews
    : public ConstrainedWebDialogDelegateBase {
 public:
  ConstrainedWebDialogDelegateViews(content::BrowserContext* context,
                                    WebDialogDelegate* delegate,
                                    WebDialogWebContentsDelegate* tab_delegate,
                                    views::WebView* view)
      : ConstrainedWebDialogDelegateBase(context, delegate, tab_delegate),
        view_(view) {}

  virtual ~ConstrainedWebDialogDelegateViews() {}

  // WebDialogWebContentsDelegate:
  virtual void CloseContents(content::WebContents* source) OVERRIDE {
    view_->GetWidget()->Close();
  }

  // contents::WebContentsDelegate:
  virtual void HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) OVERRIDE {
    unhandled_keyboard_event_handler_.HandleKeyboardEvent(
        event, view_->GetFocusManager());
  }

  // ConstrainedWebDialogDelegate:
  virtual web_modal::NativeWebContentsModalDialog GetNativeDialog() OVERRIDE {
    return view_->GetWidget()->GetNativeView();
  }

 private:
  // Converts keyboard events on the WebContents to accelerators.
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  views::WebView* view_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedWebDialogDelegateViews);
};

#if defined(USE_AURA)
// TODO(msw): Make this part of WebView? Modify various WebContentsDelegates?
class WebViewFocusHelper : public aura::client::FocusChangeObserver {
 public:
  explicit WebViewFocusHelper(views::WebView* web_view)
      : web_view_(web_view),
        window_(NULL),
        focus_client_(NULL) {
    if (web_view_ && web_view_->web_contents()) {
      content::RenderWidgetHostView* host_view =
          web_view_->web_contents()->GetRenderWidgetHostView();
      window_ = host_view ? host_view->GetNativeView() : NULL;
    }
    focus_client_ = window_ ? aura::client::GetFocusClient(window_) : NULL;
    if (focus_client_)
      focus_client_->AddObserver(this);
  }

  virtual ~WebViewFocusHelper() {
    if (focus_client_)
      focus_client_->RemoveObserver(this);
  }

  virtual void OnWindowFocused(aura::Window* gained_focus,
                               aura::Window* lost_focus) OVERRIDE {
    if (gained_focus == window_ && !web_view_->HasFocus())
      web_view_->RequestFocus();
  }

 private:
  views::WebView* web_view_;
  aura::Window* window_;
  aura::client::FocusClient* focus_client_;

  DISALLOW_COPY_AND_ASSIGN(WebViewFocusHelper);
};
#endif

class ConstrainedWebDialogDelegateViewViews
    : public views::WebView,
      public ConstrainedWebDialogDelegate,
      public views::WidgetDelegate {
 public:
  ConstrainedWebDialogDelegateViewViews(
      content::BrowserContext* browser_context,
      WebDialogDelegate* delegate,
      WebDialogWebContentsDelegate* tab_delegate)
      : views::WebView(browser_context),
        impl_(new ConstrainedWebDialogDelegateViews(browser_context, delegate,
                                                    tab_delegate, this)) {
    SetWebContents(GetWebContents());
    AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
  }
  virtual ~ConstrainedWebDialogDelegateViewViews() {}

  // ConstrainedWebDialogDelegate:
  virtual const WebDialogDelegate* GetWebDialogDelegate() const OVERRIDE {
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
  virtual web_modal::NativeWebContentsModalDialog GetNativeDialog() OVERRIDE {
    return impl_->GetNativeDialog();
  }
  virtual content::WebContents* GetWebContents() OVERRIDE {
    return impl_->GetWebContents();
  }

  // views::WidgetDelegate:
  virtual views::View* GetInitiallyFocusedView() OVERRIDE {
    return this;
  }
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
  virtual base::string16 GetWindowTitle() const OVERRIDE {
    return impl_->closed_via_webui() ? base::string16() :
        GetWebDialogDelegate()->GetDialogTitle();
  }
  virtual views::View* GetContentsView() OVERRIDE {
    return this;
  }
  virtual views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) OVERRIDE {
    return views::DialogDelegate::CreateDialogFrameView(widget);
  }
  virtual bool ShouldShowCloseButton() const OVERRIDE {
    // No close button if the dialog doesn't want a title bar.
    return impl_->GetWebDialogDelegate()->ShouldShowDialogTitle();
  }
  virtual ui::ModalType GetModalType() const OVERRIDE {
    return ui::MODAL_TYPE_CHILD;
  }

  // views::WebView:
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE {
    // Pressing ESC closes the dialog.
    DCHECK_EQ(ui::VKEY_ESCAPE, accelerator.key_code());
    GetWidget()->Close();
    return true;
  }
  virtual gfx::Size GetPreferredSize() const OVERRIDE {
    gfx::Size size;
    if (!impl_->closed_via_webui())
      GetWebDialogDelegate()->GetDialogSize(&size);
    return size;
  }
  virtual gfx::Size GetMinimumSize() const OVERRIDE {
    // Return an empty size so that we can be made smaller.
    return gfx::Size();
  }

  void OnShow() {
#if defined(USE_AURA)
  web_view_focus_helper_.reset(new WebViewFocusHelper(this));
#endif
  }

 private:
  scoped_ptr<ConstrainedWebDialogDelegateViews> impl_;
#if defined(USE_AURA)
  scoped_ptr<WebViewFocusHelper> web_view_focus_helper_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ConstrainedWebDialogDelegateViewViews);
};

}  // namespace

ConstrainedWebDialogDelegate* CreateConstrainedWebDialog(
    content::BrowserContext* browser_context,
    WebDialogDelegate* delegate,
    WebDialogWebContentsDelegate* tab_delegate,
    content::WebContents* web_contents) {
  ConstrainedWebDialogDelegateViewViews* dialog =
      new ConstrainedWebDialogDelegateViewViews(
          browser_context, delegate, tab_delegate);
  ShowWebModalDialogViews(dialog, web_contents);
  dialog->OnShow();
  return dialog;
}
