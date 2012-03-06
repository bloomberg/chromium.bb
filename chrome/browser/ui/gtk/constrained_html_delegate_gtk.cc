// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/constrained_html_ui_delegate_impl.h"

#include "chrome/browser/ui/gtk/constrained_window_gtk.h"
#include "chrome/browser/ui/gtk/tab_contents_container_gtk.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/gfx/size.h"

using content::WebContents;

class ConstrainedHtmlDelegateGtk : public ConstrainedWindowGtkDelegate,
                                   public ConstrainedHtmlUIDelegate {
 public:
  ConstrainedHtmlDelegateGtk(Profile* profile,
                             HtmlDialogUIDelegate* delegate,
                             HtmlDialogTabContentsDelegate* tab_delegate);

  virtual ~ConstrainedHtmlDelegateGtk() {}

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

  // ConstrainedWindowGtkDelegate interface
  virtual GtkWidget* GetWidgetRoot() OVERRIDE {
    return tab_contents_container_.widget();
  }
  virtual GtkWidget* GetFocusWidget() OVERRIDE {
    return tab()->web_contents()->GetContentNativeView();
  }
  virtual void DeleteDelegate() OVERRIDE {
    if (!impl_->closed_via_webui())
      GetHtmlDialogUIDelegate()->OnDialogClosed("");
    delete this;
  }
  virtual bool GetBackgroundColor(GdkColor* color) OVERRIDE {
    *color = ui::kGdkWhite;
    return true;
  }

 private:
  scoped_ptr<ConstrainedHtmlUIDelegateImpl> impl_;

  TabContentsContainerGtk tab_contents_container_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedHtmlDelegateGtk);
};

ConstrainedHtmlDelegateGtk::ConstrainedHtmlDelegateGtk(
    Profile* profile,
    HtmlDialogUIDelegate* delegate,
    HtmlDialogTabContentsDelegate* tab_delegate)
    : impl_(new ConstrainedHtmlUIDelegateImpl(profile, delegate, tab_delegate)),
      tab_contents_container_(NULL) {
  tab_contents_container_.SetTab(tab());

  gfx::Size dialog_size;
  delegate->GetDialogSize(&dialog_size);
  gtk_widget_set_size_request(GTK_WIDGET(tab_contents_container_.widget()),
                              dialog_size.width(),
                              dialog_size.height());

  gtk_widget_show_all(GetWidgetRoot());
}

// static
ConstrainedHtmlUIDelegate* ConstrainedHtmlUI::CreateConstrainedHtmlDialog(
    Profile* profile,
    HtmlDialogUIDelegate* delegate,
    HtmlDialogTabContentsDelegate* tab_delegate,
    TabContentsWrapper* overshadowed) {
  ConstrainedHtmlDelegateGtk* constrained_delegate =
      new ConstrainedHtmlDelegateGtk(profile, delegate, tab_delegate);
  ConstrainedWindow* constrained_window =
      new ConstrainedWindowGtk(overshadowed, constrained_delegate);
  constrained_delegate->set_window(constrained_window);
  return constrained_delegate;
}
