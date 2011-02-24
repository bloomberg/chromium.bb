// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/importer/import_dialog_gtk.h"

#include <string>

#include "base/utf_string_conversions.h"
#include "chrome/browser/importer/importer_data_types.h"
#include "chrome/browser/ui/gtk/accessible_widget_helper_gtk.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Returns true if the checkbox is checked.
gboolean IsChecked(GtkWidget* widget) {
  return gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
}

}   // namespace

// static
void ImportDialogGtk::Show(GtkWindow* parent, Profile* profile,
                           int initial_state) {
  new ImportDialogGtk(parent, profile, initial_state);
}

////////////////////////////////////////////////////////////////////////////////
// ImportObserver implementation:
void ImportDialogGtk::ImportCanceled() {
  ImportComplete();
}

void ImportDialogGtk::ImportComplete() {
  gtk_widget_destroy(dialog_);
  delete this;
}

ImportDialogGtk::ImportDialogGtk(GtkWindow* parent, Profile* profile,
                                 int initial_state)
    : parent_(parent),
      profile_(profile),
      importer_host_(new ImporterHost),
      ALLOW_THIS_IN_INITIALIZER_LIST(importer_list_(new ImporterList)),
      initial_state_(initial_state) {
  // Load the available source profiles.
  importer_list_->DetectSourceProfiles(this);

  // Build the dialog.
  std::string dialog_name = l10n_util::GetStringUTF8(
      IDS_IMPORT_SETTINGS_TITLE);
  dialog_ = gtk_dialog_new_with_buttons(
      dialog_name.c_str(),
      parent,
      (GtkDialogFlags) (GTK_DIALOG_MODAL | GTK_DIALOG_NO_SEPARATOR),
      GTK_STOCK_CANCEL,
      GTK_RESPONSE_REJECT,
      NULL);

  accessible_widget_helper_.reset(new AccessibleWidgetHelper(
      dialog_, profile));
  accessible_widget_helper_->SendOpenWindowNotification(dialog_name);

  importer_host_->set_parent_window(GTK_WINDOW(dialog_));

  // Add import button separately as we might need to disable it, if
  // no supported browsers found.
  import_button_ = gtk_util::AddButtonToDialog(dialog_,
      l10n_util::GetStringUTF8(IDS_IMPORT_COMMIT).c_str(),
      GTK_STOCK_APPLY, GTK_RESPONSE_ACCEPT);
  GTK_WIDGET_SET_FLAGS(import_button_, GTK_CAN_DEFAULT);
  gtk_dialog_set_default_response(GTK_DIALOG(dialog_), GTK_RESPONSE_ACCEPT);

  GtkWidget* content_area = GTK_DIALOG(dialog_)->vbox;
  gtk_box_set_spacing(GTK_BOX(content_area), gtk_util::kContentAreaSpacing);

  GtkWidget* combo_hbox = gtk_hbox_new(FALSE, gtk_util::kLabelSpacing);
  GtkWidget* from = gtk_label_new(
      l10n_util::GetStringUTF8(IDS_IMPORT_FROM_LABEL).c_str());
  gtk_box_pack_start(GTK_BOX(combo_hbox), from, FALSE, FALSE, 0);

  combo_ = gtk_combo_box_new_text();
  gtk_box_pack_start(GTK_BOX(combo_hbox), combo_, TRUE, TRUE, 0);

  gtk_box_pack_start(GTK_BOX(content_area), combo_hbox, FALSE, FALSE, 0);

  GtkWidget* vbox = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);

  GtkWidget* description = gtk_label_new(
      l10n_util::GetStringUTF8(IDS_IMPORT_ITEMS_LABEL).c_str());
  gtk_misc_set_alignment(GTK_MISC(description), 0, 0);
  gtk_box_pack_start(GTK_BOX(vbox), description, FALSE, FALSE, 0);

  bookmarks_ = gtk_check_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_IMPORT_FAVORITES_CHKBOX).c_str());
  gtk_box_pack_start(GTK_BOX(vbox), bookmarks_, FALSE, FALSE, 0);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(bookmarks_),
      (initial_state_ & importer::FAVORITES) != 0);
  g_signal_connect(bookmarks_, "toggled",
                   G_CALLBACK(OnDialogWidgetClickedThunk), this);

  search_engines_ = gtk_check_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_IMPORT_SEARCH_ENGINES_CHKBOX).c_str());
  gtk_box_pack_start(GTK_BOX(vbox), search_engines_, FALSE, FALSE, 0);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(search_engines_),
      (initial_state_ & importer::SEARCH_ENGINES) != 0);
  g_signal_connect(search_engines_, "toggled",
                   G_CALLBACK(OnDialogWidgetClickedThunk), this);

  passwords_ = gtk_check_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_IMPORT_PASSWORDS_CHKBOX).c_str());
  gtk_box_pack_start(GTK_BOX(vbox), passwords_, FALSE, FALSE, 0);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(passwords_),
      (initial_state_ & importer::PASSWORDS) != 0);
  g_signal_connect(passwords_, "toggled",
                   G_CALLBACK(OnDialogWidgetClickedThunk), this);

  history_ = gtk_check_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_IMPORT_HISTORY_CHKBOX).c_str());
  gtk_box_pack_start(GTK_BOX(vbox), history_, FALSE, FALSE, 0);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(history_),
      (initial_state_ & importer::HISTORY) !=0);
  g_signal_connect(history_, "toggled",
                   G_CALLBACK(OnDialogWidgetClickedThunk), this);

  gtk_box_pack_start(GTK_BOX(content_area), vbox, FALSE, FALSE, 0);

  // Let the user know profiles are being loaded.
  gtk_combo_box_append_text(GTK_COMBO_BOX(combo_),
      l10n_util::GetStringUTF8(IDS_IMPORT_LOADING_PROFILES).c_str());
  gtk_combo_box_set_active(GTK_COMBO_BOX(combo_), 0);

  // Disable controls until source profiles are loaded.
  SetDialogControlsSensitive(false);

  g_signal_connect(dialog_, "response",
                   G_CALLBACK(OnDialogResponseThunk), this);

  UpdateDialogButtons();

  gtk_util::ShowDialogWithLocalizedSize(dialog_,
                                        IDS_IMPORT_DIALOG_WIDTH_CHARS,
                                        -1,  // height
                                        false);  // resizable
}

ImportDialogGtk::~ImportDialogGtk() {
  if (importer_list_)
    importer_list_->SetObserver(NULL);
}

void ImportDialogGtk::SourceProfilesLoaded() {
  // Detect any supported browsers that we can import from and fill
  // up the combo box. If none found, disable all controls except cancel.
  int profiles_count = importer_list_->GetAvailableProfileCount();
  SetDialogControlsSensitive(profiles_count != 0);
  gtk_combo_box_remove_text(GTK_COMBO_BOX(combo_), 0);
  if (profiles_count > 0) {
    for (int i = 0; i < profiles_count; i++) {
      std::wstring profile = importer_list_->GetSourceProfileNameAt(i);
      gtk_combo_box_append_text(GTK_COMBO_BOX(combo_),
                                WideToUTF8(profile).c_str());
    }
    gtk_widget_grab_focus(import_button_);
  } else {
    gtk_combo_box_append_text(GTK_COMBO_BOX(combo_),
      l10n_util::GetStringUTF8(IDS_IMPORT_NO_PROFILE_FOUND).c_str());
  }
  gtk_combo_box_set_active(GTK_COMBO_BOX(combo_), 0);
}

void ImportDialogGtk::OnDialogResponse(GtkWidget* widget, int response) {
  gtk_widget_hide_all(dialog_);
  if (response == GTK_RESPONSE_ACCEPT) {
    uint16 items = GetCheckedItems();
    if (items == 0) {
      ImportComplete();
    } else {
      const ProfileInfo& source_profile =
          importer_list_->GetSourceProfileInfoAt(
          gtk_combo_box_get_active(GTK_COMBO_BOX(combo_)));
      StartImportingWithUI(parent_, items, importer_host_,
                           source_profile, profile_, this, false);
    }
  } else {
    ImportCanceled();
  }
}

void ImportDialogGtk::OnDialogWidgetClicked(GtkWidget* widget) {
  UpdateDialogButtons();
}

void ImportDialogGtk::UpdateDialogButtons() {
  gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog_), GTK_RESPONSE_ACCEPT,
                                    GetCheckedItems() != 0);
}

void ImportDialogGtk::SetDialogControlsSensitive(bool sensitive) {
  gtk_widget_set_sensitive(bookmarks_, sensitive);
  gtk_widget_set_sensitive(search_engines_, sensitive);
  gtk_widget_set_sensitive(passwords_, sensitive);
  gtk_widget_set_sensitive(history_, sensitive);
  gtk_widget_set_sensitive(import_button_, sensitive);
}

uint16 ImportDialogGtk::GetCheckedItems() {
  uint16 items = importer::NONE;
  if (IsChecked(bookmarks_))
    items |= importer::FAVORITES;
  if (IsChecked(search_engines_))
    items |= importer::SEARCH_ENGINES;
  if (IsChecked(passwords_))
    items |= importer::PASSWORDS;
  if (IsChecked(history_))
    items |= importer::HISTORY;
  return items;
}
