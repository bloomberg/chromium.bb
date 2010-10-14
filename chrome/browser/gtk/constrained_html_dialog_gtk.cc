// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/constrained_html_dialog.h"

#include "gfx/rect.h"
#include "chrome/browser/dom_ui/html_dialog_tab_contents_delegate.h"
#include "chrome/browser/dom_ui/html_dialog_ui.h"
#include "chrome/browser/gtk/constrained_window_gtk.h"
#include "chrome/browser/gtk/tab_contents_container_gtk.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "ipc/ipc_message.h"

class ConstrainedHtmlDialogGtk : public ConstrainedHtmlDialog,
                                 public ConstrainedWindowGtkDelegate,
                                 public HtmlDialogTabContentsDelegate {
 public:
  ConstrainedHtmlDialogGtk(Profile* profile,
                           HtmlDialogUIDelegate* delegate);

  virtual ~ConstrainedHtmlDialogGtk();

  // ConstrainedHtmlDialog -----------------------------------------------------
  virtual ConstrainedWindowDelegate* GetConstrainedWindowDelegate() {
    return this;
  }

  // ConstrainedWindowGtkDelegate ----------------------------------------------
  virtual GtkWidget* GetWidgetRoot() {
    return tab_contents_container_.widget();
  }

  virtual void DeleteDelegate() {
    delete this;
  }

  // HtmlDialogTabContentsDelegate ---------------------------------------------
  void MoveContents(TabContents* source, const gfx::Rect& pos) {}
  void ToolbarSizeChanged(TabContents* source, bool is_animating) {}
  void HandleKeyboardEvent(const NativeWebKeyboardEvent& event) {}

 private:
  TabContentsContainerGtk tab_contents_container_;
};

ConstrainedHtmlDialogGtk::ConstrainedHtmlDialogGtk(
    Profile* profile,
    HtmlDialogUIDelegate* delegate)
    : ConstrainedHtmlDialog(profile, delegate),
      HtmlDialogTabContentsDelegate(profile),
      tab_contents_container_(NULL) {
  tab_contents_ = new TabContents(profile, NULL, MSG_ROUTING_NONE, NULL, NULL);
  tab_contents_->set_delegate(this);
  tab_contents_->controller().LoadURL(delegate->GetDialogContentURL(),
                                      GURL(), PageTransition::START_PAGE);
  tab_contents_container_.SetTabContents(tab_contents_);

  gfx::Size dialog_size;
  delegate->GetDialogSize(&dialog_size);
  gtk_widget_set_size_request(GTK_WIDGET(tab_contents_container_.widget()),
                              dialog_size.width(),
                              dialog_size.height());

  InitializeDOMUI(tab_contents_->render_view_host());

  gtk_widget_show_all(GetWidgetRoot());
}

ConstrainedHtmlDialogGtk::~ConstrainedHtmlDialogGtk() {
  delete tab_contents_;
  // Prevent other accesses to |tab_contents_| (during superclass destruction,
  // for example).
  tab_contents_ = NULL;
}

// static
ConstrainedHtmlDialog* ConstrainedHtmlDialog::CreateConstrainedHTMLDialog(
    Profile* profile, HtmlDialogUIDelegate* delegate) {
  return new ConstrainedHtmlDialogGtk(profile, delegate);
}
