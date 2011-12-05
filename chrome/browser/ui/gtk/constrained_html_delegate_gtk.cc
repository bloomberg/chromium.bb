// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/constrained_html_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/gtk/constrained_window_gtk.h"
#include "chrome/browser/ui/gtk/tab_contents_container_gtk.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/webui/html_dialog_tab_contents_delegate.h"
#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/notification_source.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/gfx/rect.h"

class ConstrainedHtmlDelegateGtk : public ConstrainedWindowGtkDelegate,
                                   public HtmlDialogTabContentsDelegate,
                                   public ConstrainedHtmlUIDelegate {
 public:
  ConstrainedHtmlDelegateGtk(Profile* profile,
                             HtmlDialogUIDelegate* delegate);

  virtual ~ConstrainedHtmlDelegateGtk() {
    if (release_tab_on_close_)
      ignore_result(tab_.release());
  }

  // ConstrainedWindowGtkDelegate ----------------------------------------------
  virtual GtkWidget* GetWidgetRoot() OVERRIDE {
    return tab_contents_container_.widget();
  }
  virtual GtkWidget* GetFocusWidget() OVERRIDE {
    return tab_->tab_contents()->GetContentNativeView();
  }
  virtual void DeleteDelegate() OVERRIDE {
    if (!closed_via_webui_)
      html_delegate_->OnDialogClosed("");
    delete this;
  }
  virtual bool GetBackgroundColor(GdkColor* color) OVERRIDE {
    *color = ui::kGdkWhite;
    return true;
  }

  // ConstrainedHtmlUIDelegate -------------------------------------------------
  virtual HtmlDialogUIDelegate* GetHtmlDialogUIDelegate() OVERRIDE {
    return html_delegate_;
  }
  virtual void OnDialogCloseFromWebUI() OVERRIDE {
    closed_via_webui_ = true;
    window_->CloseConstrainedWindow();
  }
  virtual void ReleaseTabContentsOnDialogClose() OVERRIDE {
    release_tab_on_close_ = true;
  }
  virtual ConstrainedWindow* window() OVERRIDE {
    return window_;
  }
  virtual TabContentsWrapper* tab() OVERRIDE {
    return tab_.get();
  }

  // HtmlDialogTabContentsDelegate ---------------------------------------------
  virtual void HandleKeyboardEvent(
      const NativeWebKeyboardEvent& event) OVERRIDE {
  }

  void set_window(ConstrainedWindow* window) {
    window_ = window;
  }

 private:
  scoped_ptr<TabContentsWrapper> tab_;
  TabContentsContainerGtk tab_contents_container_;
  HtmlDialogUIDelegate* html_delegate_;

  // The constrained window that owns |this|. It's saved here because it needs
  // to be closed in response to the WebUI OnDialogClose callback.
  ConstrainedWindow* window_;

  // Was the dialog closed from WebUI (in which case |html_delegate_|'s
  // OnDialogClosed() method has already been called)?
  bool closed_via_webui_;

  // If true, release |tab_| on close instead of destroying it.
  bool release_tab_on_close_;
};

ConstrainedHtmlDelegateGtk::ConstrainedHtmlDelegateGtk(
    Profile* profile,
    HtmlDialogUIDelegate* delegate)
    : HtmlDialogTabContentsDelegate(profile),
      tab_contents_container_(NULL),
      html_delegate_(delegate),
      window_(NULL),
      closed_via_webui_(false),
      release_tab_on_close_(false) {
  TabContents* tab_contents =
      new TabContents(profile, NULL, MSG_ROUTING_NONE, NULL, NULL);
  tab_.reset(new TabContentsWrapper(tab_contents));
  tab_contents->set_delegate(this);

  // Set |this| as a property on the tab contents so that the ConstrainedHtmlUI
  // can get a reference to |this|.
  ConstrainedHtmlUI::GetPropertyAccessor().SetProperty(
      tab_contents->property_bag(), this);

  tab_contents->controller().LoadURL(delegate->GetDialogContentURL(),
                                     content::Referrer(),
                                     content::PAGE_TRANSITION_START_PAGE,
                                     std::string());
  tab_contents_container_.SetTab(tab_.get());

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
    TabContentsWrapper* overshadowed) {
  ConstrainedHtmlDelegateGtk* constrained_delegate =
      new ConstrainedHtmlDelegateGtk(profile, delegate);
  ConstrainedWindow* constrained_window =
      new ConstrainedWindowGtk(overshadowed, constrained_delegate);
  constrained_delegate->set_window(constrained_window);
  return constrained_delegate;
}
