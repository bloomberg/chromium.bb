// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/update_recommended_dialog.h"

#include <gtk/gtk.h>

#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/common/pref_names.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

static const int kMessageWidth = 400;

// static
void UpdateRecommendedDialog::Show(GtkWindow* parent) {
  new UpdateRecommendedDialog(parent);
}

UpdateRecommendedDialog::UpdateRecommendedDialog(GtkWindow* parent) {
  dialog_ = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(IDS_PRODUCT_NAME).c_str(),
      parent,
      static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_NO_SEPARATOR),
      l10n_util::GetStringUTF8(IDS_NOT_NOW).c_str(),
      GTK_RESPONSE_REJECT,
      l10n_util::GetStringUTF8(IDS_RELAUNCH_AND_UPDATE).c_str(),
      GTK_RESPONSE_ACCEPT,
      NULL);

  g_signal_connect(dialog_, "response", G_CALLBACK(OnResponseThunk), this);

  // Add the message text.
  std::string text(
      l10n_util::GetStringFUTF8(IDS_UPDATE_RECOMMENDED,
                                l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));
  GtkWidget* label = gtk_label_new(text.c_str());
  gtk_util::SetLabelWidth(label, kMessageWidth);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog_)->vbox), label,
                     FALSE, FALSE, 0);

  gtk_window_set_resizable(GTK_WINDOW(dialog_), FALSE);

  gtk_widget_show_all(dialog_);
}

UpdateRecommendedDialog::~UpdateRecommendedDialog() {
}

void UpdateRecommendedDialog::OnResponse(GtkWidget* dialog, int response_id) {
  gtk_widget_destroy(dialog_);

  if (response_id == GTK_RESPONSE_ACCEPT) {
    // Set the flag to restore the last session on shutdown.
    PrefService* pref_service = g_browser_process->local_state();
    pref_service->SetBoolean(prefs::kRestartLastSessionOnShutdown, true);

    BrowserList::CloseAllBrowsersAndExit();
  }

  delete this;
}
