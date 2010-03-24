// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/options/content_filter_page_gtk.h"

#include "app/l10n_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/host_content_settings_map.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/gtk/browser_window_gtk.h"
#include "chrome/browser/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/gtk/gtk_util.h"
#include "chrome/browser/gtk/options/content_exceptions_window_gtk.h"
#include "chrome/browser/gtk/options/options_layout_gtk.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

ContentFilterPageGtk::ContentFilterPageGtk(Profile* profile,
                                           ContentSettingsType content_type)
    : OptionsPageBase(profile),
      content_type_(content_type) {
  static const int kTitleIDs[CONTENT_SETTINGS_NUM_TYPES] = {
    0,  // This dialog isn't used for cookies.
    IDS_IMAGES_SETTING_LABEL,
    IDS_JS_SETTING_LABEL,
    IDS_PLUGIN_SETTING_LABEL,
    IDS_POPUP_SETTING_LABEL,
  };
  DCHECK_EQ(arraysize(kTitleIDs),
            static_cast<size_t>(CONTENT_SETTINGS_NUM_TYPES));

  OptionsLayoutBuilderGtk options_builder;
  options_builder.AddOptionGroup(
      l10n_util::GetStringUTF8(kTitleIDs[content_type_]),
      InitGroup(), true);
  page_ = options_builder.get_page_widget();
}

ContentFilterPageGtk::~ContentFilterPageGtk() {
}


GtkWidget* ContentFilterPageGtk::InitGroup() {
  GtkWidget* vbox = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);

  static const int kAllowIDs[CONTENT_SETTINGS_NUM_TYPES] = {
    0,  // This dialog isn't used for cookies.
    IDS_IMAGES_LOAD_RADIO,
    IDS_JS_ALLOW_RADIO,
    IDS_PLUGIN_LOAD_RADIO,
    IDS_POPUP_ALLOW_RADIO,
  };
  DCHECK_EQ(arraysize(kAllowIDs),
            static_cast<size_t>(CONTENT_SETTINGS_NUM_TYPES));
  allow_radio_ = gtk_radio_button_new_with_label(NULL,
      l10n_util::GetStringUTF8(kAllowIDs[content_type_]).c_str());
  gtk_box_pack_start(GTK_BOX(vbox), allow_radio_, FALSE, FALSE, 0);

  static const int kBlockIDs[CONTENT_SETTINGS_NUM_TYPES] = {
    0,  // This dialog isn't used for cookies.
    IDS_IMAGES_NOLOAD_RADIO,
    IDS_JS_DONOTALLOW_RADIO,
    IDS_PLUGIN_NOLOAD_RADIO,
    IDS_POPUP_BLOCK_RADIO,
  };
  DCHECK_EQ(arraysize(kBlockIDs),
            static_cast<size_t>(CONTENT_SETTINGS_NUM_TYPES));
  block_radio_ = gtk_radio_button_new_with_label_from_widget(
      GTK_RADIO_BUTTON(allow_radio_),
      l10n_util::GetStringUTF8(kBlockIDs[content_type_]).c_str());
  gtk_box_pack_start(GTK_BOX(vbox), block_radio_, FALSE, FALSE, 0);

  ContentSetting default_setting = profile()->GetHostContentSettingsMap()->
      GetDefaultContentSetting(content_type_);
  // Now that these have been added to the view hierarchy, it's safe to call
  // SetChecked() on them.
  if (default_setting == CONTENT_SETTING_ALLOW) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(allow_radio_), TRUE);
  } else {
    DCHECK(default_setting == CONTENT_SETTING_BLOCK);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(block_radio_), TRUE);
  }

  // Now that we've set the default value, we can connect to the radio button's
  // toggled signal.
  g_signal_connect(G_OBJECT(allow_radio_), "toggled",
                   G_CALLBACK(OnAllowToggledThunk), this);
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
         (toggle_button == block_radio_));

  bool allow = gtk_toggle_button_get_active(
      GTK_TOGGLE_BUTTON(allow_radio_));
  profile()->GetHostContentSettingsMap()->
      SetDefaultContentSetting(
          content_type_,
          allow ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK);
}

void ContentFilterPageGtk::OnExceptionsClicked(GtkWidget* button) {
  HostContentSettingsMap* settings_map =
      profile()->GetHostContentSettingsMap();
  ContentExceptionsWindowGtk::ShowExceptionsWindow(
      GTK_WINDOW(gtk_widget_get_toplevel(button)),
      settings_map, content_type_);
}

void ContentFilterPageGtk::OnPluginsPageLinkClicked(GtkWidget* button) {
  // We open a new browser window so the Options dialog doesn't get lost
  // behind other windows.
  Browser* browser = Browser::Create(profile());
  browser->OpenURL(GURL(chrome::kChromeUIPluginsURL),
                   GURL(), NEW_FOREGROUND_TAB, PageTransition::LINK);
  browser->window()->Show();
}
