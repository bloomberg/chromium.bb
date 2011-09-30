// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/constrained_html_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/gtk/constrained_window_gtk.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/views/tab_contents/tab_contents_container.h"
#include "chrome/browser/ui/webui/html_dialog_tab_contents_delegate.h"
#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/gfx/rect.h"
#include "views/widget/native_widget_gtk.h"

// ConstrainedHtmlDelegateGtk works with ConstrainedWindowGtk to present
// a TabContents in a ContraintedHtmlUI.
class ConstrainedHtmlDelegateGtk : public views::NativeWidgetGtk,
                                   public ConstrainedHtmlUIDelegate,
                                   public ConstrainedWindowGtkDelegate,
                                   public HtmlDialogTabContentsDelegate {
 public:
  ConstrainedHtmlDelegateGtk(Profile* profile,
                             HtmlDialogUIDelegate* delegate);
  ~ConstrainedHtmlDelegateGtk();

  // ConstrainedHtmlUIDelegate interface.
  virtual HtmlDialogUIDelegate* GetHtmlDialogUIDelegate() OVERRIDE;
  virtual void OnDialogCloseFromWebUI() OVERRIDE;
  virtual bool GetBackgroundColor(GdkColor* color) OVERRIDE {
    *color = ui::kGdkWhite;
    return true;
  }

  // ConstrainedWindowGtkDelegate implementation.
  virtual GtkWidget* GetWidgetRoot() OVERRIDE {
    return GetNativeView();
  }
  virtual GtkWidget* GetFocusWidget() OVERRIDE {
    return html_tab_contents_.GetContentNativeView();
  }
  virtual void DeleteDelegate() OVERRIDE {
    if (!closed_via_webui_)
      html_delegate_->OnDialogClosed("");
    tab_container_->ChangeTabContents(NULL);
  }
  virtual bool ShouldHaveBorderPadding() const OVERRIDE {
    return false;
  }

  // HtmlDialogTabContentsDelegate interface.
  void HandleKeyboardEvent(const NativeWebKeyboardEvent& event) OVERRIDE {}

  void set_window(ConstrainedWindow* window) {
    window_ = window;
  }

 private:
  TabContents html_tab_contents_;
  TabContentsContainer* tab_container_;

  HtmlDialogUIDelegate* html_delegate_;

  // The constrained window that owns |this|.  Saved so we can close it later.
  ConstrainedWindow* window_;

  // Was the dialog closed from WebUI (in which case |html_delegate_|'s
  // OnDialogClosed() method has already been called)?
  bool closed_via_webui_;
};

ConstrainedHtmlDelegateGtk::ConstrainedHtmlDelegateGtk(
    Profile* profile,
    HtmlDialogUIDelegate* delegate)
    : views::NativeWidgetGtk(new views::Widget),
      HtmlDialogTabContentsDelegate(profile),
      html_tab_contents_(profile, NULL, MSG_ROUTING_NONE, NULL, NULL),
      tab_container_(NULL),
      html_delegate_(delegate),
      window_(NULL),
      closed_via_webui_(false) {
  CHECK(delegate);
  html_tab_contents_.set_delegate(this);

  // Set |this| as a property so the ConstrainedHtmlUI can retrieve it.
  ConstrainedHtmlUI::GetPropertyAccessor().SetProperty(
      html_tab_contents_.property_bag(), this);
  html_tab_contents_.controller().LoadURL(delegate->GetDialogContentURL(),
                                          GURL(),
                                          PageTransition::START_PAGE,
                                          std::string());

  views::Widget::InitParams params(views::Widget::InitParams::TYPE_CONTROL);
  params.native_widget = this;
  GetWidget()->Init(params);

  tab_container_ = new TabContentsContainer;
  GetWidget()->SetContentsView(tab_container_);
  tab_container_->ChangeTabContents(&html_tab_contents_);

  gfx::Size dialog_size;
  html_delegate_->GetDialogSize(&dialog_size);
  gtk_widget_set_size_request(GetWidgetRoot(),
                              dialog_size.width(),
                              dialog_size.height());
}

ConstrainedHtmlDelegateGtk::~ConstrainedHtmlDelegateGtk() {
}

HtmlDialogUIDelegate* ConstrainedHtmlDelegateGtk::GetHtmlDialogUIDelegate() {
  return html_delegate_;
}

void ConstrainedHtmlDelegateGtk::OnDialogCloseFromWebUI() {
  closed_via_webui_ = true;
  window_->CloseConstrainedWindow();
}

// static
ConstrainedWindow* ConstrainedHtmlUI::CreateConstrainedHtmlDialog(
    Profile* profile,
    HtmlDialogUIDelegate* delegate,
    TabContentsWrapper* wrapper) {
  ConstrainedHtmlDelegateGtk* constrained_delegate =
      new ConstrainedHtmlDelegateGtk(profile, delegate);
  ConstrainedWindow* constrained_window =
      new ConstrainedWindowGtk(wrapper, constrained_delegate);
  constrained_delegate->set_window(constrained_window);
  return constrained_window;
}
