// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/constrained_html_ui.h"

#include "chrome/browser/dom_ui/html_dialog_tab_contents_delegate.h"
#include "chrome/browser/dom_ui/html_dialog_ui.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/gtk/constrained_window_gtk.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/tab_contents_container_gtk.h"
#include "chrome/common/notification_source.h"
#include "ipc/ipc_message.h"
#include "ui/gfx/rect.h"

class ConstrainedHtmlDelegateGtk : public ConstrainedWindowGtkDelegate,
                                   public HtmlDialogTabContentsDelegate,
                                   public ConstrainedHtmlUIDelegate {
 public:
  ConstrainedHtmlDelegateGtk(Profile* profile,
                           HtmlDialogUIDelegate* delegate);

  virtual ~ConstrainedHtmlDelegateGtk();

  // ConstrainedWindowGtkDelegate ----------------------------------------------
  virtual GtkWidget* GetWidgetRoot() {
    return tab_contents_container_.widget();
  }
  virtual GtkWidget* GetFocusWidget() {
    return tab_contents_.GetContentNativeView();
  }
  virtual void DeleteDelegate() {
    html_delegate_->OnDialogClosed("");
    delete this;
  }

  // ConstrainedHtmlDelegate ---------------------------------------------
  virtual HtmlDialogUIDelegate* GetHtmlDialogUIDelegate();
  virtual void OnDialogClose();
  virtual bool GetBackgroundColor(GdkColor* color) {
    *color = gtk_util::kGdkWhite;
    return true;
  }

  // HtmlDialogTabContentsDelegate ---------------------------------------------
  void MoveContents(TabContents* source, const gfx::Rect& pos) {}
  void ToolbarSizeChanged(TabContents* source, bool is_animating) {}
  void HandleKeyboardEvent(const NativeWebKeyboardEvent& event) {}

  void set_window(ConstrainedWindow* window) {
    window_ = window;
  }

 private:
  TabContents tab_contents_;
  TabContentsContainerGtk tab_contents_container_;
  HtmlDialogUIDelegate* html_delegate_;

  // The constrained window that owns |this|. It's saved here because it needs
  // to be closed in response to the WebUI OnDialogClose callback.
  ConstrainedWindow* window_;
};

ConstrainedHtmlDelegateGtk::ConstrainedHtmlDelegateGtk(
    Profile* profile,
    HtmlDialogUIDelegate* delegate)
    : HtmlDialogTabContentsDelegate(profile),
      tab_contents_(profile, NULL, MSG_ROUTING_NONE, NULL, NULL),
      tab_contents_container_(NULL),
      html_delegate_(delegate),
      window_(NULL) {
  tab_contents_.set_delegate(this);

  // Set |this| as a property on the tab contents so that the ConstrainedHtmlUI
  // can get a reference to |this|.
  ConstrainedHtmlUI::GetPropertyAccessor().SetProperty(
      tab_contents_.property_bag(), this);

  tab_contents_.controller().LoadURL(delegate->GetDialogContentURL(),
                                     GURL(), PageTransition::START_PAGE);
  tab_contents_container_.SetTabContents(&tab_contents_);

  gfx::Size dialog_size;
  delegate->GetDialogSize(&dialog_size);
  gtk_widget_set_size_request(GTK_WIDGET(tab_contents_container_.widget()),
                              dialog_size.width(),
                              dialog_size.height());

  gtk_widget_show_all(GetWidgetRoot());
}

ConstrainedHtmlDelegateGtk::~ConstrainedHtmlDelegateGtk() {
}

HtmlDialogUIDelegate*
    ConstrainedHtmlDelegateGtk::GetHtmlDialogUIDelegate() {
  return html_delegate_;
}

void ConstrainedHtmlDelegateGtk::OnDialogClose() {
  window_->CloseConstrainedWindow();
}

// static
void ConstrainedHtmlUI::CreateConstrainedHtmlDialog(
    Profile* profile,
    HtmlDialogUIDelegate* delegate,
    TabContents* overshadowed) {
  ConstrainedHtmlDelegateGtk* constrained_delegate =
      new ConstrainedHtmlDelegateGtk(profile, delegate);
  ConstrainedWindow* constrained_window =
      overshadowed->CreateConstrainedDialog(constrained_delegate);
  constrained_delegate->set_window(constrained_window);
}
