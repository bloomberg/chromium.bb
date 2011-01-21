// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/options/cookie_filter_page_gtk.h"

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "chrome/browser/browsing_data_local_storage_helper.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/options/content_exceptions_window_gtk.h"
#include "chrome/browser/ui/gtk/options/cookies_view.h"
#include "chrome/browser/ui/options/show_options_url.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Stick small widgets in an hbox so it doesn't expand to the whole width.
GtkWidget* WrapInHBox(GtkWidget* widget) {
  GtkWidget* hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, FALSE, 0);
  return hbox;
}

}  // namespace

CookieFilterPageGtk::CookieFilterPageGtk(Profile* profile)
    : OptionsPageBase(profile),
      ignore_toggle_(false) {
  // The pref members must be initialized first since the pref values are read
  // afterwards when we initialize the cookie storing group.
  clear_site_data_on_exit_.Init(prefs::kClearSiteDataOnExit,
                                profile->GetPrefs(), this);

  block_third_party_cookies_.Init(prefs::kBlockThirdPartyCookies,
                                  profile->GetPrefs(), this);

  managed_default_cookies_setting_.Init(prefs::kManagedDefaultCookiesSetting,
                                        profile->GetPrefs(), this);

  GtkWidget* title_label = gtk_util::CreateBoldLabel(
      l10n_util::GetStringUTF8(IDS_MODIFY_COOKIE_STORING_LABEL));
  page_ = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(page_), title_label, FALSE, FALSE, 0);
  gtk_container_add(GTK_CONTAINER(page_), InitCookieStoringGroup());
}

CookieFilterPageGtk::~CookieFilterPageGtk() {
}

void CookieFilterPageGtk::NotifyPrefChanged(const std::string* pref_name) {
  AutoReset<bool> toggle_guard(&ignore_toggle_, true);

  if (!pref_name || *pref_name == prefs::kClearSiteDataOnExit) {
    UpdateUiState();
  }
  if (!pref_name || *pref_name == prefs::kManagedDefaultCookiesSetting) {
    UpdateUiState();
  }
  if (!pref_name || *pref_name == prefs::kBlockThirdPartyCookies) {
    UpdateUiState();
  }
}

void CookieFilterPageGtk::HighlightGroup(OptionsGroup highlight_group) {
  // TODO(erg): implement group highlighting
}

GtkWidget* CookieFilterPageGtk::InitCookieStoringGroup() {
  GtkWidget* vbox = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);

  allow_radio_ = gtk_radio_button_new_with_label(NULL,
      l10n_util::GetStringUTF8(IDS_COOKIES_ALLOW_RADIO).c_str());
  g_signal_connect(G_OBJECT(allow_radio_), "toggled",
                   G_CALLBACK(OnCookiesAllowToggledThunk), this);
  gtk_box_pack_start(GTK_BOX(vbox), allow_radio_, FALSE, FALSE, 0);

  block_radio_ = gtk_radio_button_new_with_label_from_widget(
      GTK_RADIO_BUTTON(allow_radio_),
      l10n_util::GetStringUTF8(IDS_COOKIES_BLOCK_RADIO).c_str());
  g_signal_connect(G_OBJECT(block_radio_), "toggled",
                   G_CALLBACK(OnCookiesAllowToggledThunk), this);
  gtk_box_pack_start(GTK_BOX(vbox), block_radio_, FALSE, FALSE, 0);

  exceptions_button_ = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_COOKIES_EXCEPTIONS_BUTTON).c_str());
  g_signal_connect(G_OBJECT(exceptions_button_), "clicked",
                   G_CALLBACK(OnExceptionsClickedThunk), this);
  gtk_box_pack_start(GTK_BOX(vbox), WrapInHBox(exceptions_button_),
                     FALSE, FALSE, 0);

  block_3rdparty_check_ = gtk_check_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_COOKIES_BLOCK_3RDPARTY_CHKBOX).c_str());
  g_signal_connect(G_OBJECT(block_3rdparty_check_), "toggled",
                   G_CALLBACK(OnBlockThirdPartyToggledThunk), this);
  gtk_box_pack_start(GTK_BOX(vbox), block_3rdparty_check_, FALSE, FALSE, 0);

  clear_on_close_check_ = gtk_check_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_COOKIES_CLEAR_WHEN_CLOSE_CHKBOX).c_str());
  g_signal_connect(G_OBJECT(clear_on_close_check_), "toggled",
                   G_CALLBACK(OnClearOnCloseToggledThunk), this);
  gtk_box_pack_start(GTK_BOX(vbox), clear_on_close_check_, FALSE, FALSE, 0);

  show_cookies_button_ = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_COOKIES_SHOW_COOKIES_BUTTON).c_str());
  g_signal_connect(G_OBJECT(show_cookies_button_), "clicked",
                   G_CALLBACK(OnShowCookiesClickedThunk), this);
  gtk_box_pack_start(GTK_BOX(vbox), WrapInHBox(show_cookies_button_),
                     FALSE, FALSE, 0);

  GtkWidget* flash_settings_link = gtk_chrome_link_button_new(
      l10n_util::GetStringUTF8(IDS_FLASH_STORAGE_SETTINGS).c_str());
  g_signal_connect(G_OBJECT(flash_settings_link), "clicked",
                   G_CALLBACK(OnFlashLinkClickedThunk), this);
  gtk_box_pack_start(GTK_BOX(vbox), WrapInHBox(flash_settings_link),
                     FALSE, FALSE, 0);

  ignore_toggle_ = true;
  UpdateUiState();
  ignore_toggle_ = false;

  return vbox;
};

void CookieFilterPageGtk::UpdateUiState() {
  // Get default_setting.
  ContentSetting default_setting;
  default_setting = profile()->GetHostContentSettingsMap()->
      GetDefaultContentSetting(CONTENT_SETTINGS_TYPE_COOKIES);

  // Set UI state.
  if (default_setting == CONTENT_SETTING_ALLOW) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(allow_radio_), TRUE);
  } else {
    DCHECK(default_setting == CONTENT_SETTING_BLOCK);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(block_radio_), TRUE);
  }
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(clear_on_close_check_),
                               clear_site_data_on_exit_.GetValue());
  gtk_toggle_button_set_active(
      GTK_TOGGLE_BUTTON(block_3rdparty_check_),
      block_third_party_cookies_.GetValue());

  // Disable the UI if the default content setting is managed.
  gtk_widget_set_sensitive(block_3rdparty_check_,
                           !block_third_party_cookies_.IsManaged());
  bool sensitive = true;
  if (profile()->GetHostContentSettingsMap()->IsDefaultContentSettingManaged(
      CONTENT_SETTINGS_TYPE_COOKIES)) {
    sensitive = false;
  }
  gtk_widget_set_sensitive(allow_radio_, sensitive);
  gtk_widget_set_sensitive(block_radio_, sensitive);
  gtk_widget_set_sensitive(exceptions_button_, sensitive);
}

void CookieFilterPageGtk::OnCookiesAllowToggled(GtkWidget* toggle_button) {
  if (ignore_toggle_)
    return;

  if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle_button))) {
    // When selecting a radio button, we get two signals (one for the old radio
    // being toggled off, one for the new one being toggled on.)  Ignore the
    // signal for toggling off the old button.
    return;
  }

  ContentSetting setting = CONTENT_SETTING_ALLOW;
  if (toggle_button == allow_radio_)
    setting = CONTENT_SETTING_ALLOW;
  else if (toggle_button == block_radio_)
    setting = CONTENT_SETTING_BLOCK;

  profile()->GetHostContentSettingsMap()->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_COOKIES, setting);
}

void CookieFilterPageGtk::OnExceptionsClicked(GtkWidget* button) {
  HostContentSettingsMap* settings_map = profile()->GetHostContentSettingsMap();
  HostContentSettingsMap* otr_settings_map =
      profile()->HasOffTheRecordProfile() ?
          profile()->GetOffTheRecordProfile()->GetHostContentSettingsMap() :
          NULL;
  ContentExceptionsWindowGtk::ShowExceptionsWindow(
      GTK_WINDOW(gtk_widget_get_toplevel(button)),
      settings_map, otr_settings_map, CONTENT_SETTINGS_TYPE_COOKIES);
}

void CookieFilterPageGtk::OnBlockThirdPartyToggled(GtkWidget* toggle_button) {
  if (ignore_toggle_)
    return;

  HostContentSettingsMap* settings_map = profile()->GetHostContentSettingsMap();
  settings_map->SetBlockThirdPartyCookies(
      gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle_button)));
}

void CookieFilterPageGtk::OnClearOnCloseToggled(GtkWidget* toggle_button) {
  if (ignore_toggle_)
    return;

  clear_site_data_on_exit_.SetValue(
      gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle_button)));
}

void CookieFilterPageGtk::OnShowCookiesClicked(GtkWidget* button) {
  UserMetricsRecordAction(UserMetricsAction("Options_ShowCookies"), NULL);
  CookiesView::Show(GTK_WINDOW(gtk_widget_get_toplevel(button)),
                    profile(),
                    new BrowsingDataDatabaseHelper(profile()),
                    new BrowsingDataLocalStorageHelper(profile()),
                    new BrowsingDataAppCacheHelper(profile()),
                    BrowsingDataIndexedDBHelper::Create(profile()));
}

void CookieFilterPageGtk::OnFlashLinkClicked(GtkWidget* button) {
  browser::ShowOptionsURL(
      profile(), GURL(l10n_util::GetStringUTF8(IDS_FLASH_STORAGE_URL)));
}
