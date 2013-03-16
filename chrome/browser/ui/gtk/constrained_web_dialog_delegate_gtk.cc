// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/constrained_web_dialog_delegate_base.h"

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "chrome/browser/ui/gtk/constrained_window_gtk.h"
#include "chrome/browser/ui/web_contents_modal_dialog_manager.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/gtk/gtk_signal.h"
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
    gtk_widget_destroy(window_);
  }

  void set_window(GtkWidget* window) { window_ = window; }
  GtkWidget* window() const { return window_; }

 private:
  GtkWidget* window_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedWebDialogDelegateGtk);
};

void SetBackgroundColor(GtkWidget* widget, const GdkColor &color) {
  gtk_widget_modify_base(widget, GTK_STATE_NORMAL, &color);
  gtk_widget_modify_fg(widget, GTK_STATE_NORMAL, &color);
  gtk_widget_modify_bg(widget, GTK_STATE_NORMAL, &color);
}

}  // namespace

class ConstrainedWebDialogDelegateViewGtk
    : public ConstrainedWebDialogDelegate {
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
    return impl_->window();
  }
  virtual WebContents* GetWebContents() OVERRIDE {
    return impl_->GetWebContents();
  }

  void SetWindow(GtkWidget* window) {
    impl_->set_window(window);
  }

  GtkWidget* GetWindow() {
    return impl_->window();
  }

 private:
  CHROMEGTK_CALLBACK_0(ConstrainedWebDialogDelegateViewGtk, void, OnDestroy);

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
  GtkWidget* contents =
      GTK_WIDGET(GetWebContents()->GetView()->GetNativeView());
  gtk_widget_set_size_request(contents,
                              dialog_size.width(),
                              dialog_size.height());

  gtk_widget_show_all(contents);

  g_signal_connect(contents, "destroy", G_CALLBACK(OnDestroyThunk), this);
}

void ConstrainedWebDialogDelegateViewGtk::OnDestroy(GtkWidget* widget) {
  if (!impl_->closed_via_webui())
    GetWebDialogDelegate()->OnDialogClosed("");
  delete this;
}

ConstrainedWebDialogDelegate* CreateConstrainedWebDialog(
      content::BrowserContext* browser_context,
      WebDialogDelegate* delegate,
      WebDialogWebContentsDelegate* tab_delegate,
      content::WebContents* web_contents) {
  ConstrainedWebDialogDelegateViewGtk* constrained_delegate =
      new ConstrainedWebDialogDelegateViewGtk(
          browser_context, delegate, tab_delegate);
  GtkWidget* window =
      CreateWebContentsModalDialogGtk(
          constrained_delegate->GetWebContents()->GetView()->GetNativeView(),
          constrained_delegate->GetWebContents()->GetView()->
              GetContentNativeView());
  SetBackgroundColor(window, ui::kGdkWhite);
  constrained_delegate->SetWindow(window);

  WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      WebContentsModalDialogManager::FromWebContents(web_contents);
  web_contents_modal_dialog_manager->ShowDialog(window);

  return constrained_delegate;
}
