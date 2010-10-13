// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/options/content_filter_page_gtk.h"

#include "app/l10n_util.h"
#include "base/command_line.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/geolocation/geolocation_content_settings_map.h"
#include "chrome/browser/geolocation/geolocation_exceptions_table_model.h"
#include "chrome/browser/host_content_settings_map.h"
#include "chrome/browser/plugin_exceptions_table_model.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/gtk/browser_window_gtk.h"
#include "chrome/browser/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/gtk/gtk_util.h"
#include "chrome/browser/gtk/options/content_exceptions_window_gtk.h"
#include "chrome/browser/gtk/options/simple_content_exceptions_window.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/notification_exceptions_table_model.h"
#include "chrome/browser/show_options_url.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

ContentFilterPageGtk::ContentFilterPageGtk(Profile* profile,
                                           ContentSettingsType content_type)
    : OptionsPageBase(profile),
      content_type_(content_type),
      ask_radio_(NULL) {
  static const int kTitleIDs[] = {
    0,  // This dialog isn't used for cookies.
    IDS_IMAGES_SETTING_LABEL,
    IDS_JS_SETTING_LABEL,
    IDS_PLUGIN_SETTING_LABEL,
    IDS_POPUP_SETTING_LABEL,
    IDS_GEOLOCATION_SETTING_LABEL,
    IDS_NOTIFICATIONS_SETTING_LABEL,
  };
  COMPILE_ASSERT(arraysize(kTitleIDs) == CONTENT_SETTINGS_NUM_TYPES,
                 kTitleIDs_IncorrectSize);

  GtkWidget* title_label = gtk_util::CreateBoldLabel(
      l10n_util::GetStringUTF8(kTitleIDs[content_type_]));
  page_ = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(page_), title_label, FALSE, FALSE, 0);
  gtk_container_add(GTK_CONTAINER(page_), InitGroup());
}

ContentFilterPageGtk::~ContentFilterPageGtk() {
}


GtkWidget* ContentFilterPageGtk::InitGroup() {
  GtkWidget* vbox = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);

  static const int kAllowIDs[] = {
    0,  // This dialog isn't used for cookies.
    IDS_IMAGES_LOAD_RADIO,
    IDS_JS_ALLOW_RADIO,
    IDS_PLUGIN_LOAD_RADIO,
    IDS_POPUP_ALLOW_RADIO,
    IDS_GEOLOCATION_ALLOW_RADIO,
    IDS_NOTIFICATIONS_ALLOW_RADIO,
  };
  COMPILE_ASSERT(arraysize(kAllowIDs) == CONTENT_SETTINGS_NUM_TYPES,
                 kAllowIDs_IncorrectSize);
  allow_radio_ = gtk_radio_button_new_with_label(NULL,
      l10n_util::GetStringUTF8(kAllowIDs[content_type_]).c_str());
  gtk_box_pack_start(GTK_BOX(vbox), allow_radio_, FALSE, FALSE, 0);

  static const int kAskIDs[] = {
     0,  // This dialog isn't used for cookies.
     0,
     0,
     IDS_PLUGIN_ASK_RADIO,
     0,
     IDS_GEOLOCATION_ASK_RADIO,
     IDS_NOTIFICATIONS_ASK_RADIO,
  };
  COMPILE_ASSERT(arraysize(kAskIDs) == CONTENT_SETTINGS_NUM_TYPES,
                 kAskIDs_IncorrectSize);
  if (kAskIDs[content_type_]) {
    ask_radio_ = gtk_radio_button_new_with_label_from_widget(
        GTK_RADIO_BUTTON(allow_radio_),
        l10n_util::GetStringUTF8(kAskIDs[content_type_]).c_str());
    gtk_box_pack_start(GTK_BOX(vbox), ask_radio_, FALSE, FALSE, 0);
  }

  static const int kBlockIDs[] = {
    0,  // This dialog isn't used for cookies.
    IDS_IMAGES_NOLOAD_RADIO,
    IDS_JS_DONOTALLOW_RADIO,
    IDS_PLUGIN_NOLOAD_RADIO,
    IDS_POPUP_BLOCK_RADIO,
    IDS_GEOLOCATION_BLOCK_RADIO,
    IDS_NOTIFICATIONS_BLOCK_RADIO,
  };
  COMPILE_ASSERT(arraysize(kBlockIDs) == CONTENT_SETTINGS_NUM_TYPES,
                 kBlockIDs_IncorrectSize);
  block_radio_ = gtk_radio_button_new_with_label_from_widget(
      GTK_RADIO_BUTTON(allow_radio_),
      l10n_util::GetStringUTF8(kBlockIDs[content_type_]).c_str());
  gtk_box_pack_start(GTK_BOX(vbox), block_radio_, FALSE, FALSE, 0);

  ContentSetting default_setting;
  if (content_type_ == CONTENT_SETTINGS_TYPE_GEOLOCATION) {
    default_setting = profile()->GetGeolocationContentSettingsMap()->
        GetDefaultContentSetting();
  } else if (content_type_ == CONTENT_SETTINGS_TYPE_NOTIFICATIONS) {
    default_setting = profile()->GetDesktopNotificationService()->
        GetDefaultContentSetting();
  } else {
    default_setting = profile()->GetHostContentSettingsMap()->
        GetDefaultContentSetting(content_type_);
  }
  // Now that these have been added to the view hierarchy, it's safe to call
  // SetChecked() on them.
  if (default_setting == CONTENT_SETTING_ALLOW) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(allow_radio_), TRUE);
  } else if (default_setting == CONTENT_SETTING_ASK) {
    DCHECK(ask_radio_);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ask_radio_), TRUE);
  } else {
    DCHECK(default_setting == CONTENT_SETTING_BLOCK);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(block_radio_), TRUE);
  }

  // Now that we've set the default value, we can connect to the radio button's
  // toggled signal.
  g_signal_connect(G_OBJECT(allow_radio_), "toggled",
                   G_CALLBACK(OnAllowToggledThunk), this);
  if (ask_radio_) {
    g_signal_connect(G_OBJECT(ask_radio_), "toggled",
                     G_CALLBACK(OnAllowToggledThunk), this);
  }
  g_signal_connect(G_OBJECT(block_radio_), "toggled",
                   G_CALLBACK(OnAllowToggledThunk), this);

  GtkWidget* exceptions_button = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_COOKIES_EXCEPTIONS_BUTTON).c_str());
  g_signal_connect(G_OBJECT(exceptions_button), "clicked",
                   G_CALLBACK(OnExceptionsClickedThunk), this);

  GtkWidget* hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), exceptions_button, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  // Add the "Disable individual plug-ins..." link on the plug-ins page.
  if (content_type_ == CONTENT_SETTINGS_TYPE_PLUGINS) {
    GtkWidget* plugins_page_link = gtk_chrome_link_button_new(
        l10n_util::GetStringUTF8(IDS_PLUGIN_SELECTIVE_DISABLE).c_str());
    g_signal_connect(G_OBJECT(plugins_page_link), "clicked",
                     G_CALLBACK(OnPluginsPageLinkClickedThunk), this);

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), plugins_page_link, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  }

  return vbox;
}

void ContentFilterPageGtk::OnAllowToggled(GtkWidget* toggle_button) {
  DCHECK((toggle_button == allow_radio_) ||
         (toggle_button == ask_radio_) ||
         (toggle_button == block_radio_));
  ContentSetting default_setting =
      gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(allow_radio_)) ?
          CONTENT_SETTING_ALLOW :
          gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(block_radio_)) ?
              CONTENT_SETTING_BLOCK : CONTENT_SETTING_ASK;
  DCHECK(ask_radio_ || default_setting != CONTENT_SETTING_ASK);
  if (content_type_ == CONTENT_SETTINGS_TYPE_GEOLOCATION) {
    profile()->GetGeolocationContentSettingsMap()->SetDefaultContentSetting(
        default_setting);
  } else if (content_type_ == CONTENT_SETTINGS_TYPE_NOTIFICATIONS) {
    profile()->GetDesktopNotificationService()->SetDefaultContentSetting(
        default_setting);
  } else {
    profile()->GetHostContentSettingsMap()->SetDefaultContentSetting(
        content_type_, default_setting);
  }
}

void ContentFilterPageGtk::OnExceptionsClicked(GtkWidget* button) {
  if (content_type_ == CONTENT_SETTINGS_TYPE_GEOLOCATION) {
    SimpleContentExceptionsWindow::ShowExceptionsWindow(
        GTK_WINDOW(gtk_widget_get_toplevel(button)),
        new GeolocationExceptionsTableModel(
            profile()->GetGeolocationContentSettingsMap()),
        IDS_GEOLOCATION_EXCEPTION_TITLE);
    return;
  }
  if (content_type_ == CONTENT_SETTINGS_TYPE_NOTIFICATIONS) {
    SimpleContentExceptionsWindow::ShowExceptionsWindow(
        GTK_WINDOW(gtk_widget_get_toplevel(button)),
        new NotificationExceptionsTableModel(
            profile()->GetDesktopNotificationService()),
        IDS_NOTIFICATIONS_EXCEPTION_TITLE);
    return;
  }
  HostContentSettingsMap* settings_map =
      profile()->GetHostContentSettingsMap();
  HostContentSettingsMap* otr_settings_map =
      profile()->HasOffTheRecordProfile() ?
          profile()->GetOffTheRecordProfile()->GetHostContentSettingsMap() :
          NULL;
  if (content_type_ == CONTENT_SETTINGS_TYPE_PLUGINS &&
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableResourceContentSettings)) {
    PluginExceptionsTableModel* model =
        new PluginExceptionsTableModel(settings_map, otr_settings_map);
    model->LoadSettings();
    SimpleContentExceptionsWindow::ShowExceptionsWindow(
        GTK_WINDOW(gtk_widget_get_toplevel(button)),
        model,
        IDS_PLUGINS_EXCEPTION_TITLE);
    return;
  }
  ContentExceptionsWindowGtk::ShowExceptionsWindow(
      GTK_WINDOW(gtk_widget_get_toplevel(button)),
      settings_map, otr_settings_map, content_type_);
}

void ContentFilterPageGtk::OnPluginsPageLinkClicked(GtkWidget* button) {
  browser::ShowOptionsURL(profile(), GURL(chrome::kChromeUIPluginsURL));
}
