// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/sync_setup_wizard_gtk.h"

#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/views/sync/sync_setup_wizard.h"
#include "chrome/common/gtk_util.h"

SyncSetupWizard::SyncSetupWizard(ProfileSyncService* service)
    : username_textbox_(NULL),
      password_textbox_(NULL),
      service_(service),
      visible_(false) {
}

SyncSetupWizard::~SyncSetupWizard() {
}

void SyncSetupWizard::Step(State advance_state) {
  switch (advance_state) {
    case GAIA_LOGIN:
      SyncSetupWizardGtk::Show(service_, this);
      break;
    case GAIA_SUCCESS:
    case MERGE_AND_SYNC:
    case FATAL_ERROR:
    case DONE:
      // TODO(zork): Implement
      break;
    default:
      NOTREACHED();
  }
}

bool SyncSetupWizard::IsVisible() const {
  return visible_;
}

// static
void SyncSetupWizardGtk::Show(ProfileSyncService* service,
                              SyncSetupWizard *wizard) {
  Browser* b = BrowserList::GetLastActive();
  new SyncSetupWizardGtk(b->window()->GetNativeHandle(), service, wizard);
}

SyncSetupWizardGtk::SyncSetupWizardGtk(GtkWindow* parent,
                                       ProfileSyncService* service,
                                       SyncSetupWizard *wizard)
    : service_(service),
      wizard_(wizard) {
  // TODO(zork): Put in proper localized strings.
  //
  // Build the dialog.
  GtkWidget* dialog = gtk_dialog_new_with_buttons(
      "Setting up Bookmarks Sync",
      parent,
      (GtkDialogFlags) (GTK_DIALOG_MODAL | GTK_DIALOG_NO_SEPARATOR),
      GTK_STOCK_CANCEL,
      GTK_RESPONSE_REJECT,
      NULL);
  gtk_util::AddButtonToDialog(dialog,
      "Accept",
      GTK_STOCK_APPLY, GTK_RESPONSE_ACCEPT);

  GtkWidget* content_area = GTK_DIALOG(dialog)->vbox;
  gtk_box_set_spacing(GTK_BOX(content_area), gtk_util::kContentAreaSpacing);

  GtkWidget* vbox = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_container_add(GTK_CONTAINER(content_area), vbox);

  // Description.
  GtkWidget* description = gtk_label_new(
        "Google Chrome can store your bookmark data with your Google account.\n"
        "Bookmarks that you create on this computer will instantly be made\n"
        "available on all the computers synced to this account.\n");
  gtk_misc_set_alignment(GTK_MISC(description), 0, 0);
  gtk_container_add(GTK_CONTAINER(content_area), description);

  // Label on top of the username.
  GtkWidget* email = gtk_label_new("Email:");
  gtk_misc_set_alignment(GTK_MISC(email), 0, 0);
  gtk_container_add(GTK_CONTAINER(content_area), email);

  // Username text box
  username_textbox_= gtk_entry_new();
  gtk_container_add(GTK_CONTAINER(content_area), username_textbox_);

  // Label on top of the password.
  GtkWidget* password = gtk_label_new("Password:");
  gtk_misc_set_alignment(GTK_MISC(password), 0, 0);
  gtk_container_add(GTK_CONTAINER(content_area), password);

  // Password text box
  password_textbox_= gtk_entry_new();
  gtk_entry_set_visibility(GTK_ENTRY(password_textbox_), FALSE);
  gtk_container_add(GTK_CONTAINER(content_area), password_textbox_);

  g_signal_connect(dialog, "response",
                   G_CALLBACK(HandleOnResponseDialog), this);
  gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
  gtk_widget_show_all(dialog);
}

void SyncSetupWizardGtk::OnDialogResponse(GtkWidget* widget, int response) {
  wizard_->SetVisible(false);
  if (response == GTK_RESPONSE_ACCEPT) {
    service_->OnUserSubmittedAuth(
        gtk_entry_get_text(GTK_ENTRY(username_textbox_)),
        gtk_entry_get_text(GTK_ENTRY(password_textbox_)));

    service_->SetSyncSetupCompleted();
  }
  service_->OnUserCancelledDialog();

  delete this;
  gtk_widget_destroy(GTK_WIDGET(widget));
}
