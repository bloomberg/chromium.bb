// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webui/constrained_html_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/gtk/constrained_window_gtk.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/views/tab_contents/tab_contents_container.h"
#include "chrome/browser/webui/html_dialog_tab_contents_delegate.h"
#include "chrome/browser/webui/html_dialog_ui.h"
#include "ipc/ipc_message.h"
#include "ui/gfx/rect.h"
#include "views/widget/widget_gtk.h"

// ConstrainedHtmlDelegateGtk works with ConstrainedWindowGtk to present
// a TabContents in a ContraintedHtmlUI.
class ConstrainedHtmlDelegateGtk : public views::WidgetGtk,
                                   public ConstrainedHtmlUIDelegate,
                                   public ConstrainedWindowDelegate,
                                   public HtmlDialogTabContentsDelegate {
 public:
  ConstrainedHtmlDelegateGtk(Profile* profile,
                             HtmlDialogUIDelegate* delegate);
  ~ConstrainedHtmlDelegateGtk();

  // ConstrainedHtmlUIDelegate interface.
  virtual HtmlDialogUIDelegate* GetHtmlDialogUIDelegate();
  virtual void OnDialogClose();
  virtual bool GetBackgroundColor(GdkColor* color) {
    *color = gtk_util::kGdkWhite;
    return true;
  }

  // ConstrainedWindowGtkDelegate implementation.
  virtual GtkWidget* GetWidgetRoot() {
    return GetNativeView();
  }
  virtual GtkWidget* GetFocusWidget() {
    return html_tab_contents_.GetContentNativeView();
  }
  virtual void DeleteDelegate() {
    html_delegate_->OnDialogClosed("");
    tab_container_->ChangeTabContents(NULL);
  }

  // HtmlDialogTabContentsDelegate interface.
  void MoveContents(TabContents* source, const gfx::Rect& pos) {}
  void ToolbarSizeChanged(TabContents* source, bool is_animating) {}
  void HandleKeyboardEvent(const NativeWebKeyboardEvent& event) {}

  void set_window(ConstrainedWindow* window) {
    window_ = window;
  }

 private:
  TabContents html_tab_contents_;
  TabContentsContainer* tab_container_;

  HtmlDialogUIDelegate* html_delegate_;

  // The constrained window that owns |this|.  Saved so we can close it later.
  ConstrainedWindow* window_;
};

ConstrainedHtmlDelegateGtk::ConstrainedHtmlDelegateGtk(
    Profile* profile,
    HtmlDialogUIDelegate* delegate)
    : views::WidgetGtk(views::WidgetGtk::TYPE_CHILD),
      HtmlDialogTabContentsDelegate(profile),
      html_tab_contents_(profile, NULL, MSG_ROUTING_NONE, NULL, NULL),
      tab_container_(NULL),
      html_delegate_(delegate),
      window_(NULL) {
  CHECK(delegate);
  html_tab_contents_.set_delegate(this);

  // Set |this| as a property so the ConstrainedHtmlUI can retrieve it.
  ConstrainedHtmlUI::GetPropertyAccessor().SetProperty(
      html_tab_contents_.property_bag(), this);
  html_tab_contents_.controller().LoadURL(delegate->GetDialogContentURL(),
                                          GURL(),
                                          PageTransition::START_PAGE);

  Init(NULL, gfx::Rect());

  tab_container_ = new TabContentsContainer;
  SetContentsView(tab_container_);
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

void ConstrainedHtmlDelegateGtk::OnDialogClose() {
  window_->CloseConstrainedWindow();
}

// static
void ConstrainedHtmlUI::CreateConstrainedHtmlDialog(
    Profile* profile,
    HtmlDialogUIDelegate* delegate,
    TabContents* container) {
  ConstrainedHtmlDelegateGtk* constrained_delegate =
      new ConstrainedHtmlDelegateGtk(profile, delegate);
  ConstrainedWindow* constrained_window =
      container->CreateConstrainedDialog(constrained_delegate);
  constrained_delegate->set_window(constrained_window);
}
