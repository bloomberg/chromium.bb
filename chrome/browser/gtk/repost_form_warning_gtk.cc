// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/repost_form_warning_gtk.h"

#include "app/gtk_util.h"
#include "app/l10n_util.h"
#include "base/message_loop.h"
#include "chrome/browser/gtk/gtk_util.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/notification_service.h"
#include "grit/generated_resources.h"

RepostFormWarningGtk::RepostFormWarningGtk(GtkWindow* parent,
                                           TabContents* tab_contents)
    : navigation_controller_(&tab_contents->controller()) {
  dialog_ = gtk_vbox_new(NULL, gtk_util::kContentAreaBorder);
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

  window_ = tab_contents->CreateConstrainedDialog(this);

  registrar_.Add(this, NotificationType::LOAD_START,
                 Source<NavigationController>(navigation_controller_));
  registrar_.Add(this, NotificationType::TAB_CLOSING,
                 Source<NavigationController>(navigation_controller_));
  registrar_.Add(this, NotificationType::RELOADING,
                 Source<NavigationController>(navigation_controller_));

}

RepostFormWarningGtk::~RepostFormWarningGtk() {
}

GtkWidget* RepostFormWarningGtk::GetWidgetRoot() {
  return dialog_;
}

void RepostFormWarningGtk::DeleteDelegate() {
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void RepostFormWarningGtk::Observe(NotificationType type,
                                   const NotificationSource& source,
                                   const NotificationDetails& details) {
  // Close the dialog if we load a page (because reloading might not apply to
  // the same page anymore) or if the tab is closed, because then we won't have
  // a navigation controller anymore.
  if (dialog_ && navigation_controller_ &&
      (type == NotificationType::LOAD_START ||
       type == NotificationType::TAB_CLOSING ||
       type == NotificationType::RELOADING)) {
    DCHECK_EQ(Source<NavigationController>(source).ptr(),
              navigation_controller_);
    OnCancel(dialog_);
  }
}

void RepostFormWarningGtk::Destroy() {
  if (dialog_) {
    gtk_widget_destroy(dialog_);
    dialog_ = NULL;
  }
  window_->CloseConstrainedWindow();
}

void RepostFormWarningGtk::OnRefresh(GtkWidget* widget) {
  if (navigation_controller_) {
    navigation_controller_->ContinuePendingReload();
    // reloading the page will close the dialog.
  }
}

void RepostFormWarningGtk::OnCancel(GtkWidget* widget) {
  if (navigation_controller_) {
    navigation_controller_->CancelPendingReload();
  }
  Destroy();
}

void RepostFormWarningGtk::OnHierarchyChanged(GtkWidget* root,
                                              GtkWidget* previous_toplevel) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  if (!GTK_WIDGET_TOPLEVEL(gtk_widget_get_toplevel(cancel_))) {
    return;
  }
  gtk_widget_grab_focus(cancel_);
}

