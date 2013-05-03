// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/importer/import_lock_dialog_gtk.h"

#include <gtk/gtk.h>

#include "base/bind.h"
#include "base/message_loop.h"
#include "chrome/browser/importer/importer_lock_dialog.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "content/public/browser/user_metrics.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/l10n/l10n_util.h"

using content::UserMetricsAction;

namespace importer {

void ShowImportLockDialog(gfx::NativeWindow parent,
                          const base::Callback<void(bool)>& callback) {
  ImportLockDialogGtk::Show(parent, callback);
  content::RecordAction(UserMetricsAction("ImportLockDialogGtk_Shown"));
}

}  // namespace importer

// static
void ImportLockDialogGtk::Show(GtkWindow* parent,
                               const base::Callback<void(bool)>& callback) {
  new ImportLockDialogGtk(parent, callback);
}

ImportLockDialogGtk::ImportLockDialogGtk(
    GtkWindow* parent,
    const base::Callback<void(bool)>& callback) : callback_(callback) {
  // Build the dialog.
  dialog_ = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(IDS_IMPORTER_LOCK_TITLE).c_str(),
      parent,
      (GtkDialogFlags) (GTK_DIALOG_MODAL | GTK_DIALOG_NO_SEPARATOR),
      NULL);

  gtk_util::AddButtonToDialog(dialog_,
      l10n_util::GetStringUTF8(IDS_IMPORTER_LOCK_CANCEL).c_str(),
      GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT);
  gtk_util::AddButtonToDialog(dialog_,
      l10n_util::GetStringUTF8(IDS_IMPORTER_LOCK_OK).c_str(),
      GTK_STOCK_OK, GTK_RESPONSE_ACCEPT);

  GtkWidget* content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog_));
  gtk_box_set_spacing(GTK_BOX(content_area), ui::kContentAreaSpacing);
  GtkWidget* label = gtk_label_new(
      l10n_util::GetStringUTF8(IDS_IMPORTER_LOCK_TEXT).c_str());
  gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
  gtk_box_pack_start(GTK_BOX(content_area), label, FALSE, FALSE, 0);

  g_signal_connect(dialog_, "response", G_CALLBACK(OnResponseThunk), this);
  gtk_window_set_resizable(GTK_WINDOW(dialog_), FALSE);
  gtk_widget_show_all(dialog_);
}

ImportLockDialogGtk::~ImportLockDialogGtk() {}

void ImportLockDialogGtk::OnResponse(GtkWidget* dialog, int response_id) {
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback_, response_id == GTK_RESPONSE_ACCEPT));
  gtk_widget_destroy(dialog_);
  delete this;
}
