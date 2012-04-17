// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/constrained_html_ui_delegate_impl.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/size.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_delegate.h"

using content::WebContents;

namespace {

class ConstrainedHtmlUIDelegateImplViews
    : public ConstrainedHtmlUIDelegateImpl {
 public:
  ConstrainedHtmlUIDelegateImplViews(
      Profile* profile,
      HtmlDialogUIDelegate* delegate,
      HtmlDialogTabContentsDelegate* tab_delegate)
      : ConstrainedHtmlUIDelegateImpl(profile, delegate, tab_delegate) {
    WebContents* web_contents = tab()->web_contents();
    if (tab_delegate) {
      set_override_tab_delegate(tab_delegate);
      web_contents->SetDelegate(tab_delegate);
    } else {
      web_contents->SetDelegate(this);
    }
  }

  virtual ~ConstrainedHtmlUIDelegateImplViews() {}

  // HtmlDialogTabContentsDelegate interface.
  virtual void CloseContents(WebContents* source) OVERRIDE {
    window()->CloseConstrainedWindow();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ConstrainedHtmlUIDelegateImplViews);
};

}  // namespace

class ConstrainedHtmlDelegateViews : public views::WebView,
                                     public ConstrainedHtmlUIDelegate,
                                     public views::WidgetDelegate {
 public:
  ConstrainedHtmlDelegateViews(Profile* profile,
                               HtmlDialogUIDelegate* delegate,
                               HtmlDialogTabContentsDelegate* tab_delegate);
  virtual ~ConstrainedHtmlDelegateViews();

  void set_window(ConstrainedWindow* window) {
    return impl_->set_window(window);
  }

  // ConstrainedHtmlUIDelegate interface
  virtual const HtmlDialogUIDelegate* GetHtmlDialogUIDelegate() const OVERRIDE {
    return impl_->GetHtmlDialogUIDelegate();
  }
  virtual HtmlDialogUIDelegate* GetHtmlDialogUIDelegate() OVERRIDE {
    return impl_->GetHtmlDialogUIDelegate();
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
  virtual TabContentsWrapper* tab() OVERRIDE {
    return impl_->tab();
  }

  // views::WidgetDelegate interface.
  virtual views::View* GetInitiallyFocusedView() OVERRIDE {
    return this;
  }
  virtual bool CanResize() const OVERRIDE { return true; }
  virtual void WindowClosing() OVERRIDE {
    if (!impl_->closed_via_webui())
      GetHtmlDialogUIDelegate()->OnDialogClosed(std::string());
  }
  virtual views::Widget* GetWidget() OVERRIDE {
    return View::GetWidget();
  }
  virtual const views::Widget* GetWidget() const OVERRIDE {
    return View::GetWidget();
  }
  virtual string16 GetWindowTitle() const OVERRIDE {
    return GetHtmlDialogUIDelegate()->GetDialogTitle();
  }
  virtual views::View* GetContentsView() OVERRIDE {
    return this;
  }

  // views::WebView overrides.
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    gfx::Size size;
    GetHtmlDialogUIDelegate()->GetDialogSize(&size);
    return size;
  }

 private:
  scoped_ptr<ConstrainedHtmlUIDelegateImplViews> impl_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedHtmlDelegateViews);
};

ConstrainedHtmlDelegateViews::ConstrainedHtmlDelegateViews(
    Profile* profile,
    HtmlDialogUIDelegate* delegate,
    HtmlDialogTabContentsDelegate* tab_delegate)
    : views::WebView(profile),
      impl_(new ConstrainedHtmlUIDelegateImplViews(profile,
                                                   delegate,
                                                   tab_delegate)) {
  SetWebContents(tab()->web_contents());
}

ConstrainedHtmlDelegateViews::~ConstrainedHtmlDelegateViews() {
}

// static
ConstrainedHtmlUIDelegate* ConstrainedHtmlUI::CreateConstrainedHtmlDialog(
    Profile* profile,
    HtmlDialogUIDelegate* delegate,
    HtmlDialogTabContentsDelegate* tab_delegate,
    TabContentsWrapper* container) {
  ConstrainedHtmlDelegateViews* constrained_delegate =
      new ConstrainedHtmlDelegateViews(profile, delegate, tab_delegate);
  ConstrainedWindow* constrained_window =
      new ConstrainedWindowViews(container, constrained_delegate);
  constrained_delegate->set_window(constrained_window);
  return constrained_delegate;
}
