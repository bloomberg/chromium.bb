// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/repost_form_warning_gtk.h"

#include "base/message_loop.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/repost_form_warning_controller.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/common/notification_type.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

RepostFormWarningGtk::RepostFormWarningGtk(GtkWindow* parent,
                                           TabContents* tab_contents)
    : controller_(new RepostFormWarningController(tab_contents)) {
  dialog_ = gtk_vbox_new(FALSE, gtk_util::kContentAreaBorder);
  gtk_box_set_spacing(GTK_BOX(dialog_), gtk_util::kContentAreaSpacing);
  GtkWidget* label = gtk_label_new(
      l10n_util::GetStringUTF8(IDS_HTTP_POST_WARNING).c_str());
  GtkWidget* image = gtk_image_new_from_stock(GTK_STOCK_DIALOG_QUESTION,
                                              GTK_ICON_SIZE_DIALOG);
  gtk_misc_set_alignment(GTK_MISC(image), 0.5, 0.0);

  gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
  gtk_label_set_selectable(GTK_LABEL(label), TRUE);

  GtkWidget *hbox = gtk_hbox_new(FALSE, gtk_util::kControlSpacing);

  gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);

  gtk_box_pack_start(GTK_BOX(dialog_), hbox, FALSE, FALSE, 0);

  GtkWidget* buttonBox = gtk_hbutton_box_new();
  gtk_button_box_set_layout(GTK_BUTTON_BOX(buttonBox), GTK_BUTTONBOX_END);
  gtk_box_set_spacing(GTK_BOX(buttonBox), gtk_util::kControlSpacing);
  gtk_box_pack_end(GTK_BOX(dialog_), buttonBox, FALSE, TRUE, 0);

  cancel_ = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
  gtk_button_set_label(GTK_BUTTON(cancel_),
                       l10n_util::GetStringUTF8(IDS_CANCEL).c_str());
  g_signal_connect(cancel_, "clicked", G_CALLBACK(OnCancelThunk), this);
  gtk_box_pack_end(GTK_BOX(buttonBox), cancel_, FALSE, TRUE, 0);

  ok_ = gtk_button_new_from_stock(GTK_STOCK_REFRESH);
  gtk_button_set_label(
      GTK_BUTTON(ok_),
      l10n_util::GetStringUTF8(IDS_HTTP_POST_WARNING_RESEND).c_str());
  g_signal_connect(ok_, "clicked", G_CALLBACK(OnRefreshThunk), this);
  gtk_box_pack_end(GTK_BOX(buttonBox), ok_, FALSE, TRUE, 0);

  g_signal_connect(cancel_, "hierarchy-changed",
                   G_CALLBACK(OnHierarchyChangedThunk), this);

  controller_->Show(this);
}

GtkWidget* RepostFormWarningGtk::GetWidgetRoot() {
  return dialog_;
}

void RepostFormWarningGtk::DeleteDelegate() {
  delete this;
}

RepostFormWarningGtk::~RepostFormWarningGtk() {
  gtk_widget_destroy(dialog_);
}

void RepostFormWarningGtk::OnRefresh(GtkWidget* widget) {
  controller_->Continue();
}

void RepostFormWarningGtk::OnCancel(GtkWidget* widget) {
  controller_->Cancel();
}

void RepostFormWarningGtk::OnHierarchyChanged(GtkWidget* root,
                                              GtkWidget* previous_toplevel) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!GTK_WIDGET_TOPLEVEL(gtk_widget_get_toplevel(cancel_))) {
    return;
  }
  gtk_widget_grab_focus(cancel_);
}
