// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/constrained_web_dialog_delegate_base.h"

#include "chrome/browser/ui/gtk/constrained_window_gtk.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/gfx/size.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_ui.h"

using content::WebContents;
using ui::WebDialogDelegate;
using ui::WebDialogWebContentsDelegate;

namespace {

class ConstrainedWebDialogDelegateGtk
    : public ConstrainedWebDialogDelegateBase {
 public:
  ConstrainedWebDialogDelegateGtk(
      content::BrowserContext* browser_context,
      WebDialogDelegate* delegate,
      WebDialogWebContentsDelegate* tab_delegate)
      : ConstrainedWebDialogDelegateBase(
            browser_context, delegate, tab_delegate) {}

  // WebDialogWebContentsDelegate interface.
  virtual void CloseContents(WebContents* source) OVERRIDE {
    window_->CloseWebContentsModalDialog();
  }

  void set_window(ConstrainedWindowGtk* window) { window_ = window; }
  ConstrainedWindowGtk* window() const { return window_; }

 private:
  ConstrainedWindowGtk* window_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedWebDialogDelegateGtk);
};

}  // namespace

class ConstrainedWebDialogDelegateViewGtk
    : public ConstrainedWindowGtkDelegate,
      public ConstrainedWebDialogDelegate {
 public:
  ConstrainedWebDialogDelegateViewGtk(
      content::BrowserContext* browser_context,
      WebDialogDelegate* delegate,
      WebDialogWebContentsDelegate* tab_delegate);

  virtual ~ConstrainedWebDialogDelegateViewGtk() {}

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
  virtual NativeWebContentsModalDialog GetNativeDialog() OVERRIDE {
    return impl_->window()->GetNativeDialog();
  }
  virtual WebContents* GetWebContents() OVERRIDE {
    return impl_->GetWebContents();
  }

  // ConstrainedWindowGtkDelegate interface
  virtual GtkWidget* GetWidgetRoot() OVERRIDE {
    return GetWebContents()->GetView()->GetNativeView();
  }
  virtual GtkWidget* GetFocusWidget() OVERRIDE {
    return GetWebContents()->GetView()->GetContentNativeView();
  }
  virtual void DeleteDelegate() OVERRIDE {
    if (!impl_->closed_via_webui())
      GetWebDialogDelegate()->OnDialogClosed("");
    delete this;
  }
  virtual bool GetBackgroundColor(GdkColor* color) OVERRIDE {
    *color = ui::kGdkWhite;
    return true;
  }

  void SetWindow(ConstrainedWindowGtk* window) {
    impl_->set_window(window);
  }

  ConstrainedWindowGtk* GetWindow() {
    return impl_->window();
  }

 private:
  scoped_ptr<ConstrainedWebDialogDelegateGtk> impl_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedWebDialogDelegateViewGtk);
};

ConstrainedWebDialogDelegateViewGtk::ConstrainedWebDialogDelegateViewGtk(
    content::BrowserContext* browser_context,
    WebDialogDelegate* delegate,
    WebDialogWebContentsDelegate* tab_delegate)
    : impl_(new ConstrainedWebDialogDelegateGtk(
          browser_context,
          delegate,
          tab_delegate)) {
  gfx::Size dialog_size;
  delegate->GetDialogSize(&dialog_size);
  gtk_widget_set_size_request(GTK_WIDGET(GetWidgetRoot()),
                              dialog_size.width(),
                              dialog_size.height());

  gtk_widget_show_all(GetWidgetRoot());
}

ConstrainedWebDialogDelegate* CreateConstrainedWebDialog(
      content::BrowserContext* browser_context,
      WebDialogDelegate* delegate,
      WebDialogWebContentsDelegate* tab_delegate,
      content::WebContents* web_contents) {
  ConstrainedWebDialogDelegateViewGtk* constrained_delegate =
      new ConstrainedWebDialogDelegateViewGtk(
          browser_context, delegate, tab_delegate);
  ConstrainedWindowGtk* window =
      new ConstrainedWindowGtk(web_contents, constrained_delegate);
  constrained_delegate->SetWindow(window);
  return constrained_delegate;
}
