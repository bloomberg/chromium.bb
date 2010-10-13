// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/constrained_html_dialog_win.h"

#include "chrome/browser/dom_ui/html_dialog_ui.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_view_host_delegate.h"
#include "chrome/browser/renderer_host/render_widget_host_view_win.h"
#include "chrome/browser/renderer_host/site_instance.h"
#include "chrome/common/bindings_policy.h"
#include "chrome/common/renderer_preferences.h"
#include "views/widget/widget_win.h"
#include "views/window/window_delegate.h"

class ConstrainedHtmlDialogHostView : public views::NativeViewHost {
 public:
  explicit ConstrainedHtmlDialogHostView(ConstrainedHtmlDialogWin* dialog)
      : dialog_(dialog),
        initialized_(false) {
  }

  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child) {
    NativeViewHost::ViewHierarchyChanged(is_add, parent, child);
    if (is_add && GetWidget() && !initialized_) {
      dialog_->Init(GetWidget()->GetNativeView());
      initialized_ = true;
    }
  }

 private:
  ConstrainedHtmlDialogWin* dialog_;
  bool initialized_;
};

ConstrainedHtmlDialogWin::ConstrainedHtmlDialogWin(
    Profile* profile,
    HtmlDialogUIDelegate* delegate)
    : ConstrainedHtmlDialog(profile, delegate) {
  CHECK(delegate);
  native_view_host_ = new ConstrainedHtmlDialogHostView(this);

  // Size the native view to match the delegate preferred size.
  gfx::Size size;
  delegate->GetDialogSize(&size);
  native_view_host_->SetPreferredSize(size);

  // Create a site instance for the correct URL.
  dialog_url_ = delegate->GetDialogContentURL();
  site_instance_ =
      SiteInstance::CreateSiteInstanceForURL(profile, dialog_url_);
}

ConstrainedHtmlDialogWin::~ConstrainedHtmlDialogWin() {
}

ConstrainedWindowDelegate*
ConstrainedHtmlDialogWin::GetConstrainedWindowDelegate() {
  return this;
}

bool ConstrainedHtmlDialogWin::CanResize() const {
  return true;
}

views::View* ConstrainedHtmlDialogWin::GetContentsView() {
  return native_view_host_;
}

void ConstrainedHtmlDialogWin::WindowClosing() {
  html_dialog_ui_delegate()->OnDialogClosed("");
}

int ConstrainedHtmlDialogWin::GetBrowserWindowID() const {
  return 0;
}

ViewType::Type ConstrainedHtmlDialogWin::GetRenderViewType() const {
  return ViewType::HTML_DIALOG_UI;
}

RendererPreferences ConstrainedHtmlDialogWin::GetRendererPrefs(
    Profile* profile) const {
  return RendererPreferences();
}

void ConstrainedHtmlDialogWin::ProcessDOMUIMessage(
    const ViewHostMsg_DomMessage_Params& params) {
  DOMUI::ProcessDOMUIMessage(params);
}

void ConstrainedHtmlDialogWin::Init(gfx::NativeView parent_view) {
  render_view_host_.reset(new RenderViewHost(site_instance_,
                                             this,
                                             MSG_ROUTING_NONE,
                                             NULL));
  render_view_host_->AllowBindings(BindingsPolicy::DOM_UI);
  render_widget_host_view_ =
      new RenderWidgetHostViewWin(render_view_host_.get());
  render_view_host_->set_view(render_widget_host_view_);
  render_view_host_->CreateRenderView(string16());
  render_view_host_->NavigateToURL(dialog_url_);
  HWND hwnd = render_widget_host_view_->Create(parent_view);
  render_widget_host_view_->ShowWindow(SW_SHOW);
  native_view_host_->Attach(hwnd);

  InitializeDOMUI(render_view_host_.get());
}

// static
ConstrainedHtmlDialog* ConstrainedHtmlDialog::CreateConstrainedHTMLDialog(
    Profile* profile, HtmlDialogUIDelegate* delegate) {
  return new ConstrainedHtmlDialogWin(profile, delegate);
}
