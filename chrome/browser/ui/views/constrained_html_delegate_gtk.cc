// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/constrained_html_ui_delegate_impl.h"

#include "chrome/browser/ui/gtk/constrained_window_gtk.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/views/tab_contents/tab_contents_container.h"
#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/gfx/size.h"
#include "ui/views/widget/native_widget_gtk.h"

using content::WebContents;

// ConstrainedHtmlDelegateGtk works with ConstrainedWindowGtk to present
// a TabContents in a ContraintedHtmlUI.
class ConstrainedHtmlDelegateGtk : public views::NativeWidgetGtk,
                                   public ConstrainedHtmlUIDelegate,
                                   public ConstrainedWindowGtkDelegate {
 public:
  ConstrainedHtmlDelegateGtk(Profile* profile,
                             HtmlDialogUIDelegate* delegate,
                             HtmlDialogTabContentsDelegate* tab_delegate);
  virtual ~ConstrainedHtmlDelegateGtk();

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

  // ConstrainedWindowGtkDelegate interface.
  virtual GtkWidget* GetWidgetRoot() OVERRIDE {
    return GetNativeView();
  }
  virtual GtkWidget* GetFocusWidget() OVERRIDE {
    return tab()->web_contents()->GetContentNativeView();
  }
  virtual void DeleteDelegate() OVERRIDE {
    if (!impl_->closed_via_webui())
      GetHtmlDialogUIDelegate()->OnDialogClosed("");
    tab_container_->ChangeWebContents(NULL);
  }
  virtual bool GetBackgroundColor(GdkColor* color) OVERRIDE {
    *color = ui::kGdkWhite;
    return true;
  }
  virtual bool ShouldHaveBorderPadding() const OVERRIDE {
    return false;
  }

 private:
  scoped_ptr<ConstrainedHtmlUIDelegateImpl> impl_;

  TabContentsContainer* tab_container_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedHtmlDelegateGtk);
};

ConstrainedHtmlDelegateGtk::ConstrainedHtmlDelegateGtk(
    Profile* profile,
    HtmlDialogUIDelegate* delegate,
    HtmlDialogTabContentsDelegate* tab_delegate)
    : views::NativeWidgetGtk(new views::Widget),
      impl_(new ConstrainedHtmlUIDelegateImpl(profile, delegate, tab_delegate)),
      tab_container_(NULL) {
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_CONTROL);
  params.native_widget = this;
  GetWidget()->Init(params);

  tab_container_ = new TabContentsContainer;
  GetWidget()->SetContentsView(tab_container_);
  tab_container_->ChangeWebContents(tab()->web_contents());

  gfx::Size dialog_size;
  GetHtmlDialogUIDelegate()->GetDialogSize(&dialog_size);
  gtk_widget_set_size_request(GetWidgetRoot(),
                              dialog_size.width(),
                              dialog_size.height());
}

ConstrainedHtmlDelegateGtk::~ConstrainedHtmlDelegateGtk() {
}

// static
ConstrainedHtmlUIDelegate* ConstrainedHtmlUI::CreateConstrainedHtmlDialog(
    Profile* profile,
    HtmlDialogUIDelegate* delegate,
    HtmlDialogTabContentsDelegate* tab_delegate,
    TabContentsWrapper* wrapper) {
  ConstrainedHtmlDelegateGtk* constrained_delegate =
      new ConstrainedHtmlDelegateGtk(profile, delegate, tab_delegate);
  ConstrainedWindow* constrained_window =
      new ConstrainedWindowGtk(wrapper, constrained_delegate);
  constrained_delegate->set_window(constrained_window);
  return constrained_delegate;
}
