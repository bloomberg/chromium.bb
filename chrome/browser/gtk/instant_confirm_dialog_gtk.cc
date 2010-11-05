// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/instant_confirm_dialog_gtk.h"

#include <gtk/gtk.h>

#include "app/l10n_util.h"
#include "chrome/browser/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/gtk/gtk_util.h"
#include "chrome/browser/instant/instant_confirm_dialog.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/show_options_url.h"
#include "chrome/common/pref_names.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"

namespace browser {

void ShowInstantConfirmDialog(GtkWindow* parent, Profile* profile) {
  new InstantConfirmDialogGtk(parent, profile);
}

}  // namespace browser

InstantConfirmDialogGtk::InstantConfirmDialogGtk(
    GtkWindow* parent, Profile* profile) : profile_(profile) {
  dialog_ = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(IDS_INSTANT_OPT_IN_TITLE).c_str(),
      parent,
      static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_NO_SEPARATOR),
      GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
      GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
      NULL);
  g_signal_connect(dialog_, "response",
                   G_CALLBACK(OnDialogResponseThunk), this);

  GtkBox* vbox = GTK_BOX(GTK_DIALOG(dialog_)->vbox);
  gtk_box_set_spacing(vbox, gtk_util::kControlSpacing);

  GtkWidget* label = gtk_label_new(
      l10n_util::GetStringUTF8(IDS_INSTANT_OPT_IN_MESSAGE).c_str());
  gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
  gtk_box_pack_start(vbox, label, FALSE, FALSE, 0);

  GtkWidget* link_button = gtk_chrome_link_button_new(
      l10n_util::GetStringUTF8(IDS_OPTIONS_LEARN_MORE_LABEL).c_str());
  g_signal_connect(link_button, "clicked",
                   G_CALLBACK(OnLinkButtonClickedThunk), this);

  GtkWidget* action_area = GTK_DIALOG(dialog_)->action_area;
  gtk_container_add(GTK_CONTAINER(action_area), link_button);
  gtk_button_box_set_child_secondary(GTK_BUTTON_BOX(action_area),
                                     link_button,
                                     TRUE);

  gtk_dialog_set_default_response(GTK_DIALOG(dialog_), GTK_RESPONSE_ACCEPT);
  gtk_widget_show_all(dialog_);
}

InstantConfirmDialogGtk::~InstantConfirmDialogGtk() {
  gtk_widget_destroy(dialog_);
}

void InstantConfirmDialogGtk::OnDialogResponse(GtkWidget* dialog,
                                               int response) {
  if (response == GTK_RESPONSE_ACCEPT) {
    PrefService* service = profile_->GetPrefs();
    if (service) {
      service->SetBoolean(prefs::kInstantEnabled, true);
      service->SetBoolean(prefs::kInstantConfirmDialogShown, true);
    }
  }

  delete this;
}

void InstantConfirmDialogGtk::OnLinkButtonClicked(GtkWidget* button) {
  browser::ShowOptionsURL(profile_, GURL(browser::kInstantLearnMoreURL));
}
