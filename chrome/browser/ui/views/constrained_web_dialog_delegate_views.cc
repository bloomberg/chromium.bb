// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/constrained_web_dialog_delegate_base.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
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
using ui::ConstrainedWebDialogDelegate;
using ui::ConstrainedWebDialogUI;
using ui::WebDialogDelegate;

namespace {

class ConstrainedWebDialogDelegateViews
    : public ConstrainedWebDialogDelegateBase {
 public:
  ConstrainedWebDialogDelegateViews(
      Profile* profile,
      WebDialogDelegate* delegate,
      WebDialogWebContentsDelegate* tab_delegate,
      views::WebView* view)
      : ConstrainedWebDialogDelegateBase(profile, delegate, tab_delegate),
        view_(view) {
    WebContents* web_contents = tab()->web_contents();
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
    window()->CloseConstrainedWindow();
  }

  // contents::WebContentsDelegate
  virtual void HandleKeyboardEvent(
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
      Profile* profile,
      WebDialogDelegate* delegate,
      WebDialogWebContentsDelegate* tab_delegate);
  virtual ~ConstrainedWebDialogDelegateViewViews();

  void set_window(ConstrainedWindow* window) {
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
  virtual void ReleaseTabContentsOnDialogClose() OVERRIDE {
    return impl_->ReleaseTabContentsOnDialogClose();
  }
  virtual ConstrainedWindow* window() OVERRIDE {
    return impl_->window();
  }
  virtual TabContents* tab() OVERRIDE {
    return impl_->tab();
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

  // views::WebView overrides.
  virtual bool AcceleratorPressed(
      const ui::Accelerator& accelerator) OVERRIDE {
    // Pressing ESC closes the dialog.
    DCHECK_EQ(ui::VKEY_ESCAPE, accelerator.key_code());
    window()->CloseConstrainedWindow();
    return true;
  }
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    gfx::Size size;
    if (!impl_->closed_via_webui())
      GetWebDialogDelegate()->GetDialogSize(&size);
    return size;
  }

 private:
  scoped_ptr<ConstrainedWebDialogDelegateViews> impl_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedWebDialogDelegateViewViews);
};

ConstrainedWebDialogDelegateViewViews::ConstrainedWebDialogDelegateViewViews(
    Profile* profile,
    WebDialogDelegate* delegate,
    WebDialogWebContentsDelegate* tab_delegate)
    : views::WebView(profile),
      impl_(new ConstrainedWebDialogDelegateViews(profile,
                                                  delegate,
                                                  tab_delegate,
                                                  this)) {
  SetWebContents(tab()->web_contents());

  // Pressing ESC closes the dialog.
  set_allow_accelerators(true);
  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
}

ConstrainedWebDialogDelegateViewViews::~ConstrainedWebDialogDelegateViewViews() {
}

ConstrainedWebDialogDelegate* ui::CreateConstrainedWebDialog(
    Profile* profile,
    WebDialogDelegate* delegate,
    WebDialogWebContentsDelegate* tab_delegate,
    TabContents* container) {
  ConstrainedWebDialogDelegateViewViews* constrained_delegate =
      new ConstrainedWebDialogDelegateViewViews(profile, delegate, tab_delegate);
  ConstrainedWindow* constrained_window =
      new ConstrainedWindowViews(container, constrained_delegate);
  constrained_delegate->set_window(constrained_window);
  return constrained_delegate;
}
