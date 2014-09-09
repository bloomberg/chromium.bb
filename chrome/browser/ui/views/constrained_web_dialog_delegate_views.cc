// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/constrained_web_dialog_delegate_base.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace {

class WebDialogWebContentsDelegateViews
    : public ui::WebDialogWebContentsDelegate {
 public:
  WebDialogWebContentsDelegateViews(content::BrowserContext* browser_context,
                                    content::WebContents* initiator,
                                    views::WebView* web_view)
      : ui::WebDialogWebContentsDelegate(browser_context,
                                         new ChromeWebContentsHandler()),
        initiator_(initiator),
        web_view_(web_view) {
  }
  virtual ~WebDialogWebContentsDelegateViews() {}

  // ui::WebDialogWebContentsDelegate:
  virtual void WebContentsFocused(content::WebContents* contents) OVERRIDE {
    // Ensure the WebView is focused when its WebContents is focused.
    web_view_->RequestFocus();
  }
  virtual void HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) OVERRIDE {
    // Forward shortcut keys in dialog to the browser. http://crbug.com/104586
    // Disabled on Mac due to http://crbug.com/112173
#if !defined(OS_MACOSX)
    Browser* current_browser = chrome::FindBrowserWithWebContents(initiator_);
    if (!current_browser)
      return;
    current_browser->window()->HandleKeyboardEvent(event);
#endif
  }

 private:
  content::WebContents* initiator_;
  views::WebView* web_view_;

  DISALLOW_COPY_AND_ASSIGN(WebDialogWebContentsDelegateViews);
};

class ConstrainedWebDialogDelegateViews
    : public ConstrainedWebDialogDelegateBase {
 public:
  ConstrainedWebDialogDelegateViews(content::BrowserContext* context,
                                    ui::WebDialogDelegate* delegate,
                                    content::WebContents* web_contents,
                                    views::WebView* view)
      : ConstrainedWebDialogDelegateBase(context, delegate,
            new WebDialogWebContentsDelegateViews(context, web_contents, view)),
        view_(view) {}

  virtual ~ConstrainedWebDialogDelegateViews() {}

  // ui::WebDialogWebContentsDelegate:
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

class ConstrainedWebDialogDelegateViewViews
    : public views::WebView,
      public ConstrainedWebDialogDelegate,
      public views::WidgetDelegate {
 public:
  ConstrainedWebDialogDelegateViewViews(
      content::BrowserContext* browser_context,
      ui::WebDialogDelegate* delegate,
      content::WebContents* web_contents)
      : views::WebView(browser_context),
        impl_(new ConstrainedWebDialogDelegateViews(browser_context, delegate,
                                                    web_contents, this)) {
    SetWebContents(GetWebContents());
    AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
  }
  virtual ~ConstrainedWebDialogDelegateViewViews() {}

  // ConstrainedWebDialogDelegate:
  virtual const ui::WebDialogDelegate* GetWebDialogDelegate() const OVERRIDE {
    return impl_->GetWebDialogDelegate();
  }
  virtual ui::WebDialogDelegate* GetWebDialogDelegate() OVERRIDE {
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

 private:
  scoped_ptr<ConstrainedWebDialogDelegateViews> impl_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedWebDialogDelegateViewViews);
};

}  // namespace

ConstrainedWebDialogDelegate* CreateConstrainedWebDialog(
    content::BrowserContext* browser_context,
    ui::WebDialogDelegate* delegate,
    content::WebContents* web_contents) {
  ConstrainedWebDialogDelegateViewViews* dialog =
      new ConstrainedWebDialogDelegateViewViews(
          browser_context, delegate, web_contents);
  ShowWebModalDialogViews(dialog, web_contents);
  return dialog;
}
