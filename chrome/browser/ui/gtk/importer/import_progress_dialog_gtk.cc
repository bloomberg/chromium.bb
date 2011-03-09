// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/importer/import_progress_dialog_gtk.h"

#include <gtk/gtk.h>

#include <string>

#include "base/utf_string_conversions.h"
#include "chrome/browser/importer/importer_host.h"
#include "chrome/browser/importer/importer_observer.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

void SetItemImportStatus(GtkWidget* label, int res_id, bool is_done) {
  std::string status = l10n_util::GetStringUTF8(res_id);
  // Windows version of this has fancy throbbers to indicate progress. Here
  // we rely on text until we can have something equivalent on Linux.
  if (is_done)
    status = "\xE2\x9C\x94 " + status;  // U+2714 HEAVY CHECK MARK
  else
    status.append(" ...");
  gtk_label_set_text(GTK_LABEL(label), status.c_str());
}

}  // namespace

// static
void ImportProgressDialogGtk::StartImport(
    GtkWindow* parent,
    uint16 items,
    ImporterHost* importer_host,
    ImporterObserver* importer_observer,
    const importer::ProfileInfo& browser_profile,
    Profile* profile,
    bool first_run) {
  ImportProgressDialogGtk* dialog = new ImportProgressDialogGtk(
      parent,
      items,
      importer_host,
      importer_observer,
      WideToUTF16(browser_profile.description),
      browser_profile.browser_type == importer::BOOKMARKS_HTML);

  // In headless mode it means that we don't show the progress window, but it
  // still need it to exist. No user interaction will be required.
  if (!importer_host->is_headless())
    dialog->ShowDialog();

  importer_host->StartImportSettings(
      browser_profile, profile, items, new ProfileWriter(profile), first_run);
}

ImportProgressDialogGtk::ImportProgressDialogGtk(
    GtkWindow* parent,
    uint16 items,
    ImporterHost* importer_host,
    ImporterObserver* importer_observer,
    const string16& source_profile,
    bool bookmarks_import)
    : parent_(parent),
      items_(items),
      importer_host_(importer_host),
      importer_observer_(importer_observer),
      importing_(true) {
  importer_host_->SetObserver(this);

  // Build the dialog.
  dialog_ = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(IDS_IMPORT_PROGRESS_TITLE).c_str(),
      parent_,
      (GtkDialogFlags) (GTK_DIALOG_MODAL | GTK_DIALOG_NO_SEPARATOR),
      GTK_STOCK_CANCEL,
      GTK_RESPONSE_REJECT,
      NULL);
  importer_host_->set_parent_window(GTK_WINDOW(dialog_));

  GtkWidget* content_area = GTK_DIALOG(dialog_)->vbox;
  gtk_box_set_spacing(GTK_BOX(content_area), gtk_util::kContentAreaSpacing);

  GtkWidget* control_group = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);

  GtkWidget* import_info = gtk_label_new(
      l10n_util::GetStringFUTF8(IDS_IMPORT_PROGRESS_INFO,
                                source_profile).c_str());
  gtk_util::SetLabelWidth(import_info, 400);
  gtk_box_pack_start(GTK_BOX(control_group), import_info, FALSE, FALSE, 0);

  GtkWidget* item_box = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);

  if (items_ & importer::HISTORY) {
    history_ = gtk_label_new(
        l10n_util::GetStringUTF8(IDS_IMPORT_PROGRESS_STATUS_HISTORY).c_str());
    gtk_misc_set_alignment(GTK_MISC(history_), 0, 0.5);
    gtk_box_pack_start(GTK_BOX(item_box), history_, FALSE, FALSE, 0);
  }

  if (items_ & importer::FAVORITES) {
    bookmarks_ = gtk_label_new(
        l10n_util::GetStringUTF8(IDS_IMPORT_PROGRESS_STATUS_BOOKMARKS).c_str());
    gtk_misc_set_alignment(GTK_MISC(bookmarks_), 0, 0.5);
    gtk_box_pack_start(GTK_BOX(item_box), bookmarks_, FALSE, FALSE, 0);
  }

  if (items_ & importer::SEARCH_ENGINES) {
    search_engines_ = gtk_label_new(
        l10n_util::GetStringUTF8(IDS_IMPORT_PROGRESS_STATUS_SEARCH).c_str());
    gtk_misc_set_alignment(GTK_MISC(search_engines_), 0, 0.5);
    gtk_box_pack_start(GTK_BOX(item_box), search_engines_, FALSE, FALSE, 0);
  }

  if (items_ & importer::PASSWORDS) {
    passwords_ = gtk_label_new(
        l10n_util::GetStringUTF8(IDS_IMPORT_PROGRESS_STATUS_PASSWORDS).c_str());
    gtk_misc_set_alignment(GTK_MISC(passwords_), 0, 0.5);
    gtk_box_pack_start(GTK_BOX(item_box), passwords_, FALSE, FALSE, 0);
  }

  gtk_box_pack_start(GTK_BOX(control_group), gtk_util::IndentWidget(item_box),
                     FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(content_area), control_group, FALSE, FALSE, 0);

  g_signal_connect(dialog_, "response",
                   G_CALLBACK(OnResponseThunk), this);
  gtk_window_set_resizable(GTK_WINDOW(dialog_), FALSE);
}

ImportProgressDialogGtk::~ImportProgressDialogGtk() {}

void ImportProgressDialogGtk::OnResponse(GtkWidget* dialog, int response_id) {
  if (!importing_) {
    CloseDialog();
    return;
  }

  // Only response to the dialog is to close it so we hide immediately.
  gtk_widget_hide_all(dialog_);
  if (response_id == GTK_RESPONSE_REJECT)
    importer_host_->Cancel();
}

void ImportProgressDialogGtk::ShowDialog() {
  gtk_widget_show_all(dialog_);
}

void ImportProgressDialogGtk::CloseDialog() {
  gtk_widget_destroy(dialog_);
  dialog_ = NULL;
  delete this;
}

void ImportProgressDialogGtk::ImportStarted() {
  importing_ = true;
}

void ImportProgressDialogGtk::ImportItemStarted(importer::ImportItem item) {
  DCHECK(items_ & item);
  switch (item) {
    case importer::FAVORITES:
      SetItemImportStatus(bookmarks_,
                          IDS_IMPORT_PROGRESS_STATUS_BOOKMARKS, false);
      break;
    case importer::SEARCH_ENGINES:
      SetItemImportStatus(search_engines_,
                          IDS_IMPORT_PROGRESS_STATUS_SEARCH, false);
      break;
    case importer::PASSWORDS:
      SetItemImportStatus(passwords_,
                          IDS_IMPORT_PROGRESS_STATUS_PASSWORDS, false);
      break;
    case importer::HISTORY:
      SetItemImportStatus(history_,
                          IDS_IMPORT_PROGRESS_STATUS_HISTORY, false);
      break;
    default:
      break;
  }
}

void ImportProgressDialogGtk::ImportItemEnded(importer::ImportItem item) {
  DCHECK(items_ & item);
  switch (item) {
    case importer::FAVORITES:
      SetItemImportStatus(bookmarks_,
                          IDS_IMPORT_PROGRESS_STATUS_BOOKMARKS, true);
      break;
    case importer::SEARCH_ENGINES:
      SetItemImportStatus(search_engines_,
                          IDS_IMPORT_PROGRESS_STATUS_SEARCH, true);
      break;
    case importer::PASSWORDS:
      SetItemImportStatus(passwords_,
                          IDS_IMPORT_PROGRESS_STATUS_PASSWORDS, true);
      break;
    case importer::HISTORY:
      SetItemImportStatus(history_,
                          IDS_IMPORT_PROGRESS_STATUS_HISTORY, true);
      break;
    default:
      break;
  }
}

void ImportProgressDialogGtk::ImportEnded() {
  importing_ = false;
  importer_host_->SetObserver(NULL);
  if (importer_observer_)
    importer_observer_->ImportCompleted();
  CloseDialog();
}

namespace importer {

void ShowImportProgressDialog(GtkWindow* parent,
                              uint16 items,
                              ImporterHost* importer_host,
                              ImporterObserver* importer_observer,
                              const ProfileInfo& browser_profile,
                              Profile* profile,
                              bool first_run) {
  DCHECK_NE(0, items);
  ImportProgressDialogGtk::StartImport(
      parent, items, importer_host, importer_observer, browser_profile, profile,
      first_run);
}

}  // namespace importer
