// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/constrained_web_dialog_delegate_base.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "components/constrained_window/constrained_window_views.h"
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
  virtual void WebContentsFocused(content::WebContents* contents) override {
    // Ensure the WebView is focused when its WebContents is focused.
    web_view_->RequestFocus();
  }
  virtual void HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override {
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
  virtual void CloseContents(content::WebContents* source) override {
    view_->GetWidget()->Close();
  }

  // contents::WebContentsDelegate:
  virtual void HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override {
    unhandled_keyboard_event_handler_.HandleKeyboardEvent(
        event, view_->GetFocusManager());
  }

  // ConstrainedWebDialogDelegate:
  virtual web_modal::NativeWebContentsModalDialog GetNativeDialog() override {
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
  virtual const ui::WebDialogDelegate* GetWebDialogDelegate() const override {
    return impl_->GetWebDialogDelegate();
  }
  virtual ui::WebDialogDelegate* GetWebDialogDelegate() override {
    return impl_->GetWebDialogDelegate();
  }
  virtual void OnDialogCloseFromWebUI() override {
    return impl_->OnDialogCloseFromWebUI();
  }
  virtual void ReleaseWebContentsOnDialogClose() override {
    return impl_->ReleaseWebContentsOnDialogClose();
  }
  virtual web_modal::NativeWebContentsModalDialog GetNativeDialog() override {
    return impl_->GetNativeDialog();
  }
  virtual content::WebContents* GetWebContents() override {
    return impl_->GetWebContents();
  }

  // views::WidgetDelegate:
  virtual views::View* GetInitiallyFocusedView() override {
    return this;
  }
  virtual void WindowClosing() override {
    if (!impl_->closed_via_webui())
      GetWebDialogDelegate()->OnDialogClosed(std::string());
  }
  virtual views::Widget* GetWidget() override {
    return View::GetWidget();
  }
  virtual const views::Widget* GetWidget() const override {
    return View::GetWidget();
  }
  virtual base::string16 GetWindowTitle() const override {
    return impl_->closed_via_webui() ? base::string16() :
        GetWebDialogDelegate()->GetDialogTitle();
  }
  virtual views::View* GetContentsView() override {
    return this;
  }
  virtual views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) override {
    return views::DialogDelegate::CreateDialogFrameView(widget);
  }
  virtual bool ShouldShowCloseButton() const override {
    // No close button if the dialog doesn't want a title bar.
    return impl_->GetWebDialogDelegate()->ShouldShowDialogTitle();
  }
  virtual ui::ModalType GetModalType() const override {
    return ui::MODAL_TYPE_CHILD;
  }

  // views::WebView:
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) override {
    // Pressing ESC closes the dialog.
    DCHECK_EQ(ui::VKEY_ESCAPE, accelerator.key_code());
    GetWidget()->Close();
    return true;
  }
  virtual gfx::Size GetPreferredSize() const override {
    gfx::Size size;
    if (!impl_->closed_via_webui())
      GetWebDialogDelegate()->GetDialogSize(&size);
    return size;
  }
  virtual gfx::Size GetMinimumSize() const override {
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
