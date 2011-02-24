// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/importer/import_progress_dialog_gtk.h"

#include <string>

#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using importer::ALL;
using importer::BOOKMARKS_HTML;
using importer::FAVORITES;
using importer::HISTORY;
using importer::HOME_PAGE;
using importer::ImportItem;
using importer::PASSWORDS;
using importer::ProfileInfo;
using importer::SEARCH_ENGINES;

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
void ImportProgressDialogGtk::StartImport(GtkWindow* parent,
                                          int16 items,
                                          ImporterHost* importer_host,
                                          const ProfileInfo& browser_profile,
                                          Profile* profile,
                                          ImportObserver* observer,
                                          bool first_run) {
  ImportProgressDialogGtk* v = new ImportProgressDialogGtk(
      WideToUTF16(browser_profile.description), items, importer_host, observer,
      parent, browser_profile.browser_type == BOOKMARKS_HTML);

  // In headless mode it means that we don't show the progress window, but it
  // still need it to exist. No user interaction will be required.
  if (!importer_host->is_headless())
    v->ShowDialog();

  importer_host->StartImportSettings(browser_profile, profile, items,
                                     new ProfileWriter(profile),
                                     first_run);
}

////////////////////////////////////////////////////////////////////////////////
// ImporterHost::Observer implementation:
void ImportProgressDialogGtk::ImportItemStarted(importer::ImportItem item) {
  DCHECK(items_ & item);
  switch (item) {
    case FAVORITES:
      SetItemImportStatus(bookmarks_,
                          IDS_IMPORT_PROGRESS_STATUS_BOOKMARKS, false);
      break;
    case SEARCH_ENGINES:
      SetItemImportStatus(search_engines_,
                          IDS_IMPORT_PROGRESS_STATUS_SEARCH, false);
      break;
    case PASSWORDS:
      SetItemImportStatus(passwords_,
                          IDS_IMPORT_PROGRESS_STATUS_PASSWORDS, false);
      break;
    case HISTORY:
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
    case FAVORITES:
      SetItemImportStatus(bookmarks_,
                          IDS_IMPORT_PROGRESS_STATUS_BOOKMARKS, true);
      break;
    case SEARCH_ENGINES:
      SetItemImportStatus(search_engines_,
                          IDS_IMPORT_PROGRESS_STATUS_SEARCH, true);
      break;
    case PASSWORDS:
      SetItemImportStatus(passwords_,
                          IDS_IMPORT_PROGRESS_STATUS_PASSWORDS, true);
      break;
    case HISTORY:
      SetItemImportStatus(history_,
                          IDS_IMPORT_PROGRESS_STATUS_HISTORY, true);
      break;
    default:
      break;
  }
}

void ImportProgressDialogGtk::ImportStarted() {
  importing_ = true;
}

void ImportProgressDialogGtk::ImportEnded() {
  importing_ = false;
  importer_host_->SetObserver(NULL);
  if (observer_)
    observer_->ImportComplete();
  CloseDialog();
}

ImportProgressDialogGtk::ImportProgressDialogGtk(const string16& source_profile,
    int16 items, ImporterHost* importer_host, ImportObserver* observer,
    GtkWindow* parent, bool bookmarks_import) :
    parent_(parent), importing_(true), observer_(observer),
    items_(items), importer_host_(importer_host) {
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

  if (items_ & HISTORY) {
    history_ = gtk_label_new(
        l10n_util::GetStringUTF8(IDS_IMPORT_PROGRESS_STATUS_HISTORY).c_str());
    gtk_misc_set_alignment(GTK_MISC(history_), 0, 0.5);
    gtk_box_pack_start(GTK_BOX(item_box), history_, FALSE, FALSE, 0);
  }

  if (items_ & FAVORITES) {
    bookmarks_ = gtk_label_new(
        l10n_util::GetStringUTF8(IDS_IMPORT_PROGRESS_STATUS_BOOKMARKS).c_str());
    gtk_misc_set_alignment(GTK_MISC(bookmarks_), 0, 0.5);
    gtk_box_pack_start(GTK_BOX(item_box), bookmarks_, FALSE, FALSE, 0);
  }

  if (items_ & SEARCH_ENGINES) {
    search_engines_ = gtk_label_new(
        l10n_util::GetStringUTF8(IDS_IMPORT_PROGRESS_STATUS_SEARCH).c_str());
    gtk_misc_set_alignment(GTK_MISC(search_engines_), 0, 0.5);
    gtk_box_pack_start(GTK_BOX(item_box), search_engines_, FALSE, FALSE, 0);
  }

  if (items_ & PASSWORDS) {
    passwords_ = gtk_label_new(
        l10n_util::GetStringUTF8(IDS_IMPORT_PROGRESS_STATUS_PASSWORDS).c_str());
    gtk_misc_set_alignment(GTK_MISC(passwords_), 0, 0.5);
    gtk_box_pack_start(GTK_BOX(item_box), passwords_, FALSE, FALSE, 0);
  }

  gtk_box_pack_start(GTK_BOX(control_group), gtk_util::IndentWidget(item_box),
                     FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(content_area), control_group, FALSE, FALSE, 0);

  g_signal_connect(dialog_, "response",
                   G_CALLBACK(HandleOnResponseDialog), this);
  gtk_window_set_resizable(GTK_WINDOW(dialog_), FALSE);
}

ImportProgressDialogGtk::~ImportProgressDialogGtk() {}

void ImportProgressDialogGtk::CloseDialog() {
  gtk_widget_destroy(dialog_);
  dialog_ = NULL;
  delete this;
}

void ImportProgressDialogGtk::OnDialogResponse(GtkWidget* widget,
                                               int response) {
  if (!importing_) {
    CloseDialog();
    return;
  }

  // Only response to the dialog is to close it so we hide immediately.
  gtk_widget_hide_all(dialog_);
  if (response == GTK_RESPONSE_REJECT)
    importer_host_->Cancel();
}

void ImportProgressDialogGtk::ShowDialog() {
  gtk_widget_show_all(dialog_);
}


void StartImportingWithUI(GtkWindow* parent,
                          uint16 items,
                          ImporterHost* importer_host,
                          const ProfileInfo& browser_profile,
                          Profile* profile,
                          ImportObserver* observer,
                          bool first_run) {
  DCHECK_NE(0, items);
  ImportProgressDialogGtk::StartImport(parent, items, importer_host,
                                       browser_profile, profile, observer,
                                       first_run);
}
