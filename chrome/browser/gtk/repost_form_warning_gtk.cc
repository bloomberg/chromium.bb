// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/repost_form_warning_gtk.h"

#include "app/l10n_util.h"
#include "base/message_loop.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/common/gtk_util.h"
#include "chrome/common/notification_service.h"
#include "grit/generated_resources.h"

RepostFormWarningGtk::RepostFormWarningGtk(
    GtkWindow* parent,
    NavigationController* navigation_controller)
    : navigation_controller_(navigation_controller) {
  dialog_ = gtk_message_dialog_new(
      parent,
      GTK_DIALOG_MODAL,
      GTK_MESSAGE_QUESTION,
      GTK_BUTTONS_NONE,
      "%s",
      l10n_util::GetStringUTF8(IDS_HTTP_POST_WARNING).c_str());
  gtk_util::ApplyMessageDialogQuirks(dialog_);
  gtk_window_set_title(GTK_WINDOW(dialog_),
      l10n_util::GetStringUTF8(IDS_HTTP_POST_WARNING_TITLE).c_str());
  gtk_util::AddButtonToDialog(dialog_,
      l10n_util::GetStringUTF8(IDS_HTTP_POST_WARNING_CANCEL).c_str(),
      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
  gtk_util::AddButtonToDialog(dialog_,
      l10n_util::GetStringUTF8(IDS_HTTP_POST_WARNING_RESEND).c_str(),
      GTK_STOCK_REFRESH, GTK_RESPONSE_OK);
  gtk_util::SetWindowIcon(GTK_WINDOW(dialog_));

  g_signal_connect(dialog_, "response", G_CALLBACK(OnResponse), this);
  g_signal_connect(dialog_, "destroy", G_CALLBACK(OnWindowDestroy), this);

  gtk_widget_show_all(dialog_);

  registrar_.Add(this, NotificationType::LOAD_START,
                 Source<NavigationController>(navigation_controller_));
  registrar_.Add(this, NotificationType::TAB_CLOSING,
                 Source<NavigationController>(navigation_controller_));
}

RepostFormWarningGtk::~RepostFormWarningGtk() {
}

void RepostFormWarningGtk::Observe(NotificationType type,
                                   const NotificationSource& source,
                                   const NotificationDetails& details) {
  // Close the dialog if we load a page (because reloading might not apply to
  // the same page anymore) or if the tab is closed, because then we won't have
  // a navigation controller anymore.
  if (dialog_ && navigation_controller_ &&
      (type == NotificationType::LOAD_START ||
       type == NotificationType::TAB_CLOSING)) {
    DCHECK_EQ(Source<NavigationController>(source).ptr(),
              navigation_controller_);
    navigation_controller_ = NULL;
    Destroy();
  }
}

void RepostFormWarningGtk::Destroy() {
  if (dialog_) {
    gtk_widget_destroy(dialog_);
    dialog_ = NULL;
  }
}

// static
void RepostFormWarningGtk::OnResponse(GtkWidget* widget, int response,
                                      RepostFormWarningGtk* dialog) {
  dialog->Destroy();
  if (response == GTK_RESPONSE_OK) {
    if (dialog->navigation_controller_)
      dialog->navigation_controller_->Reload(false);
  }
}

// static
void RepostFormWarningGtk::OnWindowDestroy(GtkWidget* widget,
                                           RepostFormWarningGtk* dialog) {
  MessageLoop::current()->DeleteSoon(FROM_HERE, dialog);
}
