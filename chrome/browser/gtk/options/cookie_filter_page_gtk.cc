// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/options/cookie_filter_page_gtk.h"

#include "app/l10n_util.h"
#include "base/command_line.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browsing_data_local_storage_helper.h"
#include "chrome/browser/gtk/browser_window_gtk.h"
#include "chrome/browser/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/gtk/gtk_util.h"
#include "chrome/browser/gtk/options/content_exceptions_window_gtk.h"
#include "chrome/browser/gtk/options/cookies_view.h"
#include "chrome/browser/host_content_settings_map.h"
#include "chrome/browser/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

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
      initializing_(true) {
  GtkWidget* title_label = gtk_util::CreateBoldLabel(
      l10n_util::GetStringUTF8(IDS_MODIFY_COOKIE_STORING_LABEL));
  page_ = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(page_), title_label, FALSE, FALSE, 0);
  gtk_container_add(GTK_CONTAINER(page_), InitCookieStoringGroup());

  clear_site_data_on_exit_.Init(prefs::kClearSiteDataOnExit,
                                profile->GetPrefs(), NULL);

  // Load initial values
  NotifyPrefChanged(NULL);
}

CookieFilterPageGtk::~CookieFilterPageGtk() {
}

void CookieFilterPageGtk::NotifyPrefChanged(const std::string* pref_name) {
  initializing_ = true;

  if (!pref_name || *pref_name == prefs::kClearSiteDataOnExit) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(clear_on_close_check_),
                                 clear_site_data_on_exit_.GetValue());
  }

  initializing_ = false;
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

  // Set up the current value for the button.
  const HostContentSettingsMap* settings_map =
      profile()->GetHostContentSettingsMap();
  ContentSetting default_setting =
      settings_map->GetDefaultContentSetting(CONTENT_SETTINGS_TYPE_COOKIES);
  // Now that these have been added to the view hierarchy, it's safe to call
  // SetChecked() on them.
  GtkWidget* radio_button = NULL;
  if (default_setting == CONTENT_SETTING_ALLOW) {
    radio_button = allow_radio_;
  } else if (default_setting == CONTENT_SETTING_BLOCK) {
    radio_button = block_radio_;
  } else {
    DCHECK(default_setting == CONTENT_SETTING_ASK);
    radio_button = ask_every_time_radio_;
  }
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button), TRUE);

  // Exception button.
  GtkWidget* exceptions_button = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_COOKIES_EXCEPTIONS_BUTTON).c_str());
  g_signal_connect(G_OBJECT(exceptions_button), "clicked",
                   G_CALLBACK(OnExceptionsClickedThunk), this);
  gtk_box_pack_start(GTK_BOX(vbox), WrapInHBox(exceptions_button),
                     FALSE, FALSE, 0);

  block_3rdparty_check_ = gtk_check_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_COOKIES_BLOCK_3RDPARTY_CHKBOX).c_str());
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(block_3rdparty_check_),
                               settings_map->BlockThirdPartyCookies());
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

  return vbox;
};

void CookieFilterPageGtk::OnCookiesAllowToggled(GtkWidget* toggle_button) {
  if (initializing_)
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
  else if (toggle_button == ask_every_time_radio_)
    setting = CONTENT_SETTING_ASK;
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
  if (initializing_)
    return;

  HostContentSettingsMap* settings_map = profile()->GetHostContentSettingsMap();
  settings_map->SetBlockThirdPartyCookies(
      gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle_button)));
}

void CookieFilterPageGtk::OnClearOnCloseToggled(GtkWidget* toggle_button) {
  if (initializing_)
    return;

  clear_site_data_on_exit_.SetValue(
      gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle_button)));
}

void CookieFilterPageGtk::OnShowCookiesClicked(GtkWidget* button) {
  UserMetricsRecordAction(UserMetricsAction("Options_ShowCookies"), NULL);
  CookiesView::Show(GTK_WINDOW(gtk_widget_get_toplevel(button)),
                    profile(),
                    new BrowsingDataDatabaseHelper(
                        profile()),
                    new BrowsingDataLocalStorageHelper(
                        profile()),
                    new BrowsingDataAppCacheHelper(
                        profile()));
}

void CookieFilterPageGtk::OnFlashLinkClicked(GtkWidget* button) {
  // We open a new browser window so the Options dialog doesn't get lost
  // behind other windows.
  Browser* browser = Browser::Create(profile());
  browser->OpenURL(GURL(l10n_util::GetStringUTF8(IDS_FLASH_STORAGE_URL)),
                   GURL(), NEW_FOREGROUND_TAB, PageTransition::LINK);
  browser->window()->Show();
}
