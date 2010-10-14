// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/clear_browsing_data_dialog_gtk.h"

#include <string>

#include "app/gtk_util.h"
#include "app/l10n_util.h"
#include "base/command_line.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browsing_data_remover.h"
#include "chrome/browser/gtk/accessible_widget_helper_gtk.h"
#include "chrome/browser/gtk/browser_window_gtk.h"
#include "chrome/browser/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/gtk/gtk_util.h"
#include "chrome/browser/gtk/options/options_layout_gtk.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

namespace {

// Returns true if the checkbox is checked.
gboolean IsChecked(GtkWidget* widget) {
  return gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
}

}  // namespace

// static
void ClearBrowsingDataDialogGtk::Show(GtkWindow* parent, Profile* profile) {
  new ClearBrowsingDataDialogGtk(parent, profile);
}

ClearBrowsingDataDialogGtk::ClearBrowsingDataDialogGtk(GtkWindow* parent,
                                                       Profile* profile)
    : clear_server_data_button_(NULL),
      clear_server_status_label_(NULL),
      profile_(profile),
      remover_(NULL) {
  DCHECK(profile);

  // Always show preferences for the original profile. Most state when off
  // the record comes from the original profile, but we explicitly use
  // the original profile to avoid potential problems.
  profile_ = profile->GetOriginalProfile();
  sync_service_ = profile_->GetProfileSyncService();

  if (sync_service_) {
    sync_service_->ResetClearServerDataState();
    sync_service_->AddObserver(this);
  }

  // Build the dialog.
  std::string dialog_name = l10n_util::GetStringUTF8(
      IDS_CLEAR_BROWSING_DATA_TITLE);
  dialog_ = gtk_dialog_new_with_buttons(
      dialog_name.c_str(),
      parent,
      (GtkDialogFlags) (GTK_DIALOG_MODAL | GTK_DIALOG_NO_SEPARATOR),
      NULL);

  GtkWidget* cancel_button = gtk_dialog_add_button(GTK_DIALOG(dialog_),
      GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);
  gtk_widget_grab_focus(cancel_button);

  accessible_widget_helper_.reset(new AccessibleWidgetHelper(dialog_, profile));
  accessible_widget_helper_->SendOpenWindowNotification(dialog_name);

  GtkWidget* content_area = GTK_DIALOG(dialog_)->vbox;
  gtk_box_set_spacing(GTK_BOX(content_area), gtk_util::kContentAreaSpacing);

  // Build the first notebook page.
  notebook_ = gtk_notebook_new();

  gtk_notebook_append_page(
      GTK_NOTEBOOK(notebook_),
      BuildClearBrowsingDataPage(),
      gtk_label_new(
          l10n_util::GetStringUTF8(IDS_CLEAR_CHROME_DATA_TAB_LABEL).c_str()));

  gtk_notebook_append_page(
      GTK_NOTEBOOK(notebook_),
      BuildOtherDataPage(),
      gtk_label_new(
          l10n_util::GetStringUTF8(IDS_CLEAR_OTHER_DATA_TAB_LABEL).c_str()));

  gtk_box_pack_start(GTK_BOX(content_area), notebook_, FALSE, FALSE, 0);

  g_signal_connect(dialog_, "response",
                   G_CALLBACK(OnDialogResponseThunk), this);

  UpdateDialogButtons();

  gtk_util::ShowModalDialogWithMinLocalizedWidth(dialog_,
      IDS_CLEARDATA_DIALOG_WIDTH_CHARS);
}

ClearBrowsingDataDialogGtk::~ClearBrowsingDataDialogGtk() {
  if (sync_service_)
    sync_service_->RemoveObserver(this);
}

GtkWidget* ClearBrowsingDataDialogGtk::BuildClearBrowsingDataPage() {
  GtkWidget* vbox = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_container_set_border_width(GTK_CONTAINER(vbox),
                                 gtk_util::kContentAreaBorder);

  // Label on top of the checkboxes.
  GtkWidget* description = gtk_label_new(
      l10n_util::GetStringUTF8(IDS_CLEAR_BROWSING_DATA_LABEL).c_str());
  gtk_misc_set_alignment(GTK_MISC(description), 0, 0);
  gtk_box_pack_start(GTK_BOX(vbox), description, FALSE, FALSE, 0);

  // History checkbox.
  del_history_checkbox_ = gtk_check_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_DEL_BROWSING_HISTORY_CHKBOX).c_str());
  gtk_box_pack_start(GTK_BOX(vbox), del_history_checkbox_, FALSE, FALSE, 0);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(del_history_checkbox_),
      profile_->GetPrefs()->GetBoolean(prefs::kDeleteBrowsingHistory));
  g_signal_connect(del_history_checkbox_, "toggled",
                   G_CALLBACK(OnDialogWidgetClickedThunk), this);

  // Downloads checkbox.
  del_downloads_checkbox_ = gtk_check_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_DEL_DOWNLOAD_HISTORY_CHKBOX).c_str());
  gtk_box_pack_start(GTK_BOX(vbox), del_downloads_checkbox_, FALSE, FALSE, 0);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(del_downloads_checkbox_),
      profile_->GetPrefs()->GetBoolean(prefs::kDeleteDownloadHistory));
  g_signal_connect(del_downloads_checkbox_, "toggled",
                   G_CALLBACK(OnDialogWidgetClickedThunk), this);

  // Cache checkbox.
  del_cache_checkbox_ = gtk_check_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_DEL_CACHE_CHKBOX).c_str());
  gtk_box_pack_start(GTK_BOX(vbox), del_cache_checkbox_, FALSE, FALSE, 0);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(del_cache_checkbox_),
      profile_->GetPrefs()->GetBoolean(prefs::kDeleteCache));
  g_signal_connect(del_cache_checkbox_, "toggled",
                   G_CALLBACK(OnDialogWidgetClickedThunk), this);

  // Cookies checkbox.
  del_cookies_checkbox_ = gtk_check_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_DEL_COOKIES_CHKBOX).c_str());
  gtk_box_pack_start(GTK_BOX(vbox), del_cookies_checkbox_, FALSE, FALSE, 0);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(del_cookies_checkbox_),
      profile_->GetPrefs()->GetBoolean(prefs::kDeleteCookies));
  g_signal_connect(del_cookies_checkbox_, "toggled",
                   G_CALLBACK(OnDialogWidgetClickedThunk), this);

  // Passwords checkbox.
  del_passwords_checkbox_ = gtk_check_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_DEL_PASSWORDS_CHKBOX).c_str());
  gtk_box_pack_start(GTK_BOX(vbox), del_passwords_checkbox_, FALSE, FALSE, 0);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(del_passwords_checkbox_),
      profile_->GetPrefs()->GetBoolean(prefs::kDeletePasswords));
  g_signal_connect(del_passwords_checkbox_, "toggled",
                   G_CALLBACK(OnDialogWidgetClickedThunk), this);

  // Form data checkbox.
  del_form_data_checkbox_ = gtk_check_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_DEL_FORM_DATA_CHKBOX).c_str());
  gtk_box_pack_start(GTK_BOX(vbox), del_form_data_checkbox_, FALSE, FALSE, 0);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(del_form_data_checkbox_),
      profile_->GetPrefs()->GetBoolean(prefs::kDeleteFormData));
  g_signal_connect(del_form_data_checkbox_, "toggled",
                   G_CALLBACK(OnDialogWidgetClickedThunk), this);

  // Create a horizontal layout for the combo box and label.
  GtkWidget* combo_hbox = gtk_hbox_new(FALSE, gtk_util::kLabelSpacing);
  GtkWidget* time_period_label_ = gtk_label_new(
      l10n_util::GetStringUTF8(IDS_CLEAR_BROWSING_DATA_TIME_LABEL).c_str());
  gtk_box_pack_start(GTK_BOX(combo_hbox), time_period_label_, FALSE, FALSE, 0);

  // Time period combo box items.
  time_period_combobox_ = gtk_combo_box_new_text();
  gtk_combo_box_append_text(GTK_COMBO_BOX(time_period_combobox_),
      l10n_util::GetStringUTF8(IDS_CLEAR_DATA_HOUR).c_str());
  gtk_combo_box_append_text(GTK_COMBO_BOX(time_period_combobox_),
      l10n_util::GetStringUTF8(IDS_CLEAR_DATA_DAY).c_str());
  gtk_combo_box_append_text(GTK_COMBO_BOX(time_period_combobox_),
      l10n_util::GetStringUTF8(IDS_CLEAR_DATA_WEEK).c_str());
  gtk_combo_box_append_text(GTK_COMBO_BOX(time_period_combobox_),
      l10n_util::GetStringUTF8(IDS_CLEAR_DATA_4WEEKS).c_str());
  gtk_combo_box_append_text(GTK_COMBO_BOX(time_period_combobox_),
      l10n_util::GetStringUTF8(IDS_CLEAR_DATA_EVERYTHING).c_str());
  gtk_combo_box_set_active(GTK_COMBO_BOX(time_period_combobox_),
      profile_->GetPrefs()->GetInteger(prefs::kDeleteTimePeriod));
  gtk_box_pack_start(GTK_BOX(combo_hbox),
                     time_period_combobox_, FALSE, FALSE, 0);
  g_signal_connect(time_period_combobox_, "changed",
                   G_CALLBACK(OnDialogWidgetClickedThunk), this);

  // Add the combo/label time period box to the vertical layout.
  gtk_box_pack_start(GTK_BOX(vbox), combo_hbox, FALSE, FALSE, 0);

  // Create a right aligned button for clearing the browsing data.
  GtkWidget* button_box = gtk_hbutton_box_new();
  gtk_button_box_set_layout(GTK_BUTTON_BOX(button_box), GTK_BUTTONBOX_END);
  clear_browsing_data_button_ = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_CLEAR_BROWSING_DATA_COMMIT).c_str());
  g_signal_connect(clear_browsing_data_button_, "clicked",
                   G_CALLBACK(OnClearBrowsingDataClickThunk), this);
  gtk_container_add(GTK_CONTAINER(button_box), clear_browsing_data_button_);
  gtk_box_pack_start(GTK_BOX(vbox), button_box, FALSE, FALSE, 0);

  return vbox;
}

GtkWidget* ClearBrowsingDataDialogGtk::BuildOtherDataPage() {
  scoped_ptr<OptionsLayoutBuilderGtk>
      options_builder(OptionsLayoutBuilderGtk::Create());

  GtkWidget* adobe_flash_vbox = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);

  GtkWidget* flash_link = AddDescriptionAndLink(
      adobe_flash_vbox,
      IDS_CLEAR_DATA_ADOBE_FLASH_DESCRIPTION,
      IDS_FLASH_STORAGE_SETTINGS);
  g_signal_connect(G_OBJECT(flash_link), "clicked",
                   G_CALLBACK(OnFlashLinkClickedThunk), this);

  options_builder->AddOptionGroup(
      l10n_util::GetStringUTF8(IDS_CLEAR_DATA_ADOBE_FLASH_TITLE),
      adobe_flash_vbox, false);

  GtkWidget* clear_sync_vbox = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);
  GtkWidget* description = gtk_label_new(l10n_util::GetStringUTF8(
      IDS_CLEAR_DATA_CLEAR_SERVER_DATA_DESCRIPTION).c_str());
  gtk_misc_set_alignment(GTK_MISC(description), 0, 0);
  gtk_label_set_line_wrap(GTK_LABEL(description), TRUE);
  gtk_box_pack_start(GTK_BOX(clear_sync_vbox), description, FALSE, FALSE, 0);

  GtkWidget* button_hbox = gtk_hbox_new(FALSE, gtk_util::kControlSpacing);

  clear_server_data_button_ = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_CLEAR_DATA_CLEAR_BUTTON).c_str());
  g_signal_connect(clear_server_data_button_, "clicked",
                   G_CALLBACK(OnClearSyncDataClickThunk), this);
  gtk_box_pack_start(GTK_BOX(button_hbox), clear_server_data_button_,
                     FALSE, FALSE, 0);

  clear_server_status_label_ = gtk_label_new(NULL);
  gtk_box_pack_start(GTK_BOX(button_hbox), clear_server_status_label_,
                     FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(clear_sync_vbox), button_hbox,
                     FALSE, FALSE, 0);

  GtkWidget* dashboard_link = AddDescriptionAndLink(
      clear_sync_vbox,
      IDS_CLEAR_DASHBOARD_DESCRIPTION,
      IDS_SYNC_PRIVACY_DASHBOARD_LINK_LABEL);
  g_signal_connect(G_OBJECT(dashboard_link), "clicked",
                   G_CALLBACK(OnPrivacyLinkClickedThunk), this);

  options_builder->AddOptionGroup(
      l10n_util::GetStringUTF8(IDS_CLEAR_DATA_CHROME_SYNC_TITLE),
      clear_sync_vbox, false);

  UpdateClearButtonState(false);

  return options_builder->get_page_widget();
}

GtkWidget* ClearBrowsingDataDialogGtk::AddDescriptionAndLink(GtkWidget* box,
                                                             int description,
                                                             int link) {
  GtkWidget* description_label = gtk_label_new(l10n_util::GetStringUTF8(
      description).c_str());
  gtk_misc_set_alignment(GTK_MISC(description_label), 0, 0);
  gtk_label_set_line_wrap(GTK_LABEL(description_label), TRUE);
  gtk_box_pack_start(GTK_BOX(box), description_label, FALSE, FALSE, 0);

  GtkWidget* link_widget = gtk_chrome_link_button_new(
      l10n_util::GetStringUTF8(link).c_str());

  // Stick it in an hbox so it doesn't expand to the whole width.
  GtkWidget* link_hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(link_hbox), link_widget, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(box), link_hbox, FALSE, FALSE, 0);

  return link_widget;
}

void ClearBrowsingDataDialogGtk::OnDialogResponse(GtkWidget* widget,
                                                  int response) {
  delete this;
  gtk_widget_destroy(GTK_WIDGET(widget));
}

void ClearBrowsingDataDialogGtk::OnClearBrowsingDataClick(GtkWidget* widget) {
  PrefService* prefs = profile_->GetPrefs();
  prefs->SetBoolean(prefs::kDeleteBrowsingHistory,
                    IsChecked(del_history_checkbox_));
  prefs->SetBoolean(prefs::kDeleteDownloadHistory,
                    IsChecked(del_downloads_checkbox_));
  prefs->SetBoolean(prefs::kDeleteCache,
                    IsChecked(del_cache_checkbox_));
  prefs->SetBoolean(prefs::kDeleteCookies,
                    IsChecked(del_cookies_checkbox_));
  prefs->SetBoolean(prefs::kDeletePasswords,
                    IsChecked(del_passwords_checkbox_));
  prefs->SetBoolean(prefs::kDeleteFormData,
                    IsChecked(del_form_data_checkbox_));
  prefs->SetInteger(prefs::kDeleteTimePeriod, gtk_combo_box_get_active(
      GTK_COMBO_BOX(time_period_combobox_)));

  int period_selected = gtk_combo_box_get_active(
      GTK_COMBO_BOX(time_period_combobox_));

  // BrowsingDataRemover deletes itself when done.
  remover_ = new BrowsingDataRemover(
      profile_,
      static_cast<BrowsingDataRemover::TimePeriod>(period_selected),
      base::Time());
  remover_->Remove(GetCheckedItems());

  OnDialogResponse(dialog_, GTK_RESPONSE_CLOSE);
}

void ClearBrowsingDataDialogGtk::OnClearSyncDataClick(GtkWidget* widget) {
  GtkWidget* confirm = gtk_message_dialog_new(
      GTK_WINDOW(dialog_),
      static_cast<GtkDialogFlags>(
          GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
      GTK_MESSAGE_QUESTION,
      GTK_BUTTONS_YES_NO,
      "%s",
      l10n_util::GetStringUTF8(IDS_CONFIRM_CLEAR_DESCRIPTION).c_str());
  gtk_util::ApplyMessageDialogQuirks(confirm);
  gtk_window_set_title(GTK_WINDOW(confirm), l10n_util::GetStringUTF8(
      IDS_CONFIRM_CLEAR_TITLE).c_str());
  g_signal_connect(confirm, "response",
                   G_CALLBACK(OnClearSyncDataConfirmResponseThunk), this);
  gtk_widget_show_all(confirm);
}

void ClearBrowsingDataDialogGtk::OnClearSyncDataConfirmResponse(
    GtkWidget* confirm,
    gint response) {
  gtk_widget_destroy(confirm);
  if (response != GTK_RESPONSE_YES)
    return;

  sync_service_->ClearServerData();
  OnStateChanged();
}

void ClearBrowsingDataDialogGtk::OnStateChanged() {
  // Succeeded/FailedClearingServerData should only be called once, not every
  // time the view is refreshed.  As such, on success/failure handle that state
  // and immediately reset things back to CLEAR_NOT_STARTED.
  ProfileSyncService::ClearServerDataState clear_state =
      sync_service_ ?
      sync_service_->GetClearServerDataState() :
      ProfileSyncService::CLEAR_NOT_STARTED;

  if (sync_service_)
    sync_service_->ResetClearServerDataState();

  bool delete_in_progress = false;
  switch (clear_state) {
    case ProfileSyncService::CLEAR_NOT_STARTED:
      // This can occur on a first start and after a failed clear (which does
      // not close the tab). Do nothing.
      break;
    case ProfileSyncService::CLEAR_CLEARING:
      // Clearing buttons on all tabs are disabled at this point, throbber is
      // going.
      gtk_widget_set_sensitive(GTK_WIDGET(dialog_), FALSE);
      gtk_label_set_text(
          GTK_LABEL(clear_server_status_label_),
          l10n_util::GetStringUTF8(IDS_CLEAR_DATA_SENDING).c_str());
      delete_in_progress = true;
      break;
    case ProfileSyncService::CLEAR_FAILED:
      // Show an error and reallow clearing.
      gtk_widget_set_sensitive(GTK_WIDGET(dialog_), TRUE);
      gtk_label_set_text(
          GTK_LABEL(clear_server_status_label_),
          l10n_util::GetStringUTF8(IDS_CLEAR_DATA_ERROR).c_str());
      delete_in_progress = false;
      break;
    case ProfileSyncService::CLEAR_SUCCEEDED:
      // Close the dialog box, success!
      OnDialogResponse(dialog_, GTK_RESPONSE_CLOSE);
      return;
  }

  // allow_clear can be false when a local browsing data clear is happening
  // from the neighboring tab.  |delete_in_progress| means that a clear is
  // pending in the current tab.
  UpdateClearButtonState(delete_in_progress);

  // TODO(erg): There is no decent throbber widget available to us. For the
  // time being, just disable the box while doing this.
}

void ClearBrowsingDataDialogGtk::OnDialogWidgetClicked(GtkWidget* widget) {
  UpdateDialogButtons();
}

void ClearBrowsingDataDialogGtk::OnFlashLinkClicked(GtkWidget* button) {
  // We open a new browser window so the Options dialog doesn't get lost
  // behind other windows.
  Browser* browser = Browser::Create(profile_);
  browser->OpenURL(GURL(l10n_util::GetStringUTF8(IDS_FLASH_STORAGE_URL)),
                   GURL(), NEW_FOREGROUND_TAB, PageTransition::LINK);
  browser->window()->Show();
}

void ClearBrowsingDataDialogGtk::OnPrivacyLinkClicked(GtkWidget* button) {
  Browser* browser = Browser::Create(profile_);
  browser->OpenURL(GURL(l10n_util::GetStringUTF8(IDS_PRIVACY_DASHBOARD_URL)),
                   GURL(), NEW_FOREGROUND_TAB, PageTransition::LINK);
  browser->window()->Show();
}

void ClearBrowsingDataDialogGtk::UpdateDialogButtons() {
  gtk_widget_set_sensitive(GTK_WIDGET(clear_browsing_data_button_),
                           GetCheckedItems() != 0);
}

int ClearBrowsingDataDialogGtk::GetCheckedItems() {
  int items = 0;
  if (IsChecked(del_history_checkbox_))
    items |= BrowsingDataRemover::REMOVE_HISTORY;
  if (IsChecked(del_downloads_checkbox_))
    items |= BrowsingDataRemover::REMOVE_DOWNLOADS;
  if (IsChecked(del_cookies_checkbox_))
    items |= BrowsingDataRemover::REMOVE_COOKIES;
  if (IsChecked(del_passwords_checkbox_))
    items |= BrowsingDataRemover::REMOVE_PASSWORDS;
  if (IsChecked(del_form_data_checkbox_))
    items |= BrowsingDataRemover::REMOVE_FORM_DATA;
  if (IsChecked(del_cache_checkbox_))
    items |= BrowsingDataRemover::REMOVE_CACHE;
  return items;
}

void ClearBrowsingDataDialogGtk::UpdateClearButtonState(
    bool delete_in_progress) {
  gtk_widget_set_sensitive(clear_server_data_button_,
                           sync_service_ != NULL &&
                           sync_service_->HasSyncSetupCompleted() &&
                           !delete_in_progress);
}
