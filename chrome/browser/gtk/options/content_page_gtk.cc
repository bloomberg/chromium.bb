// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/options/content_page_gtk.h"

#include "app/gfx/gtk_util.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/gtk/clear_browsing_data_dialog_gtk.h"
#include "chrome/browser/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "chrome/browser/gtk/import_dialog_gtk.h"
#include "chrome/browser/gtk/options/options_layout_gtk.h"
#include "chrome/browser/gtk/options/passwords_exceptions_window_gtk.h"
#include "chrome/common/gtk_util.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/url_constants.h"
#include "grit/app_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

///////////////////////////////////////////////////////////////////////////////
// ContentPageGtk, public:

ContentPageGtk::ContentPageGtk(Profile* profile)
    : OptionsPageBase(profile),
      initializing_(true) {

  // Prepare the group options layout.
  OptionsLayoutBuilderGtk options_builder;
  options_builder.AddOptionGroup(
      l10n_util::GetStringUTF8(IDS_OPTIONS_PASSWORDS_GROUP_NAME),
      InitPasswordSavingGroup(), false);
  options_builder.AddOptionGroup(
      l10n_util::GetStringUTF8(IDS_AUTOFILL_SETTING_WINDOWS_GROUP_NAME),
      InitFormAutofillGroup(), false);
  options_builder.AddOptionGroup(
      l10n_util::GetStringUTF8(IDS_OPTIONS_BROWSING_DATA_GROUP_NAME),
      InitBrowsingDataGroup(), false);
  options_builder.AddOptionGroup(
      l10n_util::GetStringUTF8(IDS_APPEARANCE_GROUP_NAME),
      InitThemesGroup(), false);
  page_ = options_builder.get_page_widget();

  // Add preferences observers.
  ask_to_save_passwords_.Init(prefs::kPasswordManagerEnabled,
                              profile->GetPrefs(), this);
  ask_to_save_form_autofill_.Init(prefs::kFormAutofillEnabled,
                                  profile->GetPrefs(), this);
  if (browser_defaults::kCanToggleSystemTitleBar) {
    use_custom_chrome_frame_.Init(prefs::kUseCustomChromeFrame,
                                  profile->GetPrefs(), this);
  }

  // Load initial values.
  NotifyPrefChanged(NULL);

  registrar_.Add(this, NotificationType::BROWSER_THEME_CHANGED,
                 NotificationService::AllSources());
  ObserveThemeChanged();
}

ContentPageGtk::~ContentPageGtk() {
}

///////////////////////////////////////////////////////////////////////////////
// ContentPageGtk, private:

// If |pref_name| is NULL, set the state of all the widgets. (This is used
// in ContentPageGtk() above to initialize the dialog.) Otherwise, reset the
// state of the widget for the given preference name, as it has changed.
void ContentPageGtk::NotifyPrefChanged(const std::wstring* pref_name) {
  initializing_ = true;
  if (!pref_name || *pref_name == prefs::kPasswordManagerEnabled) {
    if (ask_to_save_passwords_.GetValue()) {
      gtk_toggle_button_set_active(
          GTK_TOGGLE_BUTTON(passwords_asktosave_radio_), TRUE);
    } else {
      gtk_toggle_button_set_active(
          GTK_TOGGLE_BUTTON(passwords_neversave_radio_), TRUE);
    }
  }
  if (!pref_name || *pref_name == prefs::kFormAutofillEnabled) {
    if (ask_to_save_form_autofill_.GetValue()) {
      gtk_toggle_button_set_active(
          GTK_TOGGLE_BUTTON(form_autofill_asktosave_radio_), TRUE);
    } else {
      gtk_toggle_button_set_active(
          GTK_TOGGLE_BUTTON(form_autofill_neversave_radio_), TRUE);
    }
  }
  if (browser_defaults::kCanToggleSystemTitleBar &&
      (!pref_name || *pref_name == prefs::kUseCustomChromeFrame)) {
    if (use_custom_chrome_frame_.GetValue()) {
      gtk_toggle_button_set_active(
          GTK_TOGGLE_BUTTON(system_title_bar_hide_radio_), TRUE);
    } else {
      gtk_toggle_button_set_active(
          GTK_TOGGLE_BUTTON(system_title_bar_show_radio_), TRUE);
    }
  }
  initializing_ = false;
}

void ContentPageGtk::Observe(NotificationType type,
                             const NotificationSource& source,
                             const NotificationDetails& details) {
  if (type == NotificationType::BROWSER_THEME_CHANGED)
    ObserveThemeChanged();
  else
    OptionsPageBase::Observe(type, source, details);
}

void ContentPageGtk::ObserveThemeChanged() {
#if defined(TOOLKIT_GTK)
  GtkThemeProvider* provider = GtkThemeProvider::GetFrom(profile());
  bool is_gtk_theme = provider->UseGtkTheme();
  gtk_widget_set_sensitive(gtk_theme_button_, !is_gtk_theme);
#else
  BrowserThemeProvider* provider =
      reinterpret_cast<BrowserThemeProvider*>(profile()->GetThemeProvider());
  bool is_gtk_theme = false;
#endif

  bool is_classic_theme = !is_gtk_theme && provider->GetThemeID().empty();
  gtk_widget_set_sensitive(themes_reset_button_, !is_classic_theme);
}

GtkWidget* ContentPageGtk::InitPasswordSavingGroup() {
  GtkWidget* vbox = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);

  // Ask to save radio button.
  passwords_asktosave_radio_ = gtk_radio_button_new_with_label(NULL,
      l10n_util::GetStringUTF8(IDS_OPTIONS_PASSWORDS_ASKTOSAVE).c_str());
  g_signal_connect(G_OBJECT(passwords_asktosave_radio_), "toggled",
                   G_CALLBACK(OnPasswordRadioToggled), this);
  gtk_box_pack_start(GTK_BOX(vbox), passwords_asktosave_radio_, FALSE,
                     FALSE, 0);

  // Never save radio button.
  passwords_neversave_radio_ = gtk_radio_button_new_with_label_from_widget(
      GTK_RADIO_BUTTON(passwords_asktosave_radio_),
      l10n_util::GetStringUTF8(IDS_OPTIONS_PASSWORDS_NEVERSAVE).c_str());
  g_signal_connect(G_OBJECT(passwords_neversave_radio_), "toggled",
                   G_CALLBACK(OnPasswordRadioToggled), this);
  gtk_box_pack_start(GTK_BOX(vbox), passwords_neversave_radio_, FALSE,
                     FALSE, 0);

  // Add the exceptions button into its own horizontal box so it does not
  // depend on the spacing above.
  GtkWidget* button_hbox = gtk_hbox_new(FALSE, gtk_util::kLabelSpacing);
  gtk_container_add(GTK_CONTAINER(vbox), button_hbox);
  GtkWidget* passwords_exceptions_button = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_OPTIONS_PASSWORDS_EXCEPTIONS).c_str());
  g_signal_connect(G_OBJECT(passwords_exceptions_button), "clicked",
                   G_CALLBACK(OnPasswordsExceptionsButtonClicked), this);
  gtk_box_pack_start(GTK_BOX(button_hbox), passwords_exceptions_button, FALSE,
                     FALSE, 0);

  return vbox;
}

GtkWidget* ContentPageGtk::InitFormAutofillGroup() {
  GtkWidget* vbox = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);

  // Ask to save radio button.
  form_autofill_asktosave_radio_ = gtk_radio_button_new_with_label(NULL,
      l10n_util::GetStringUTF8(IDS_OPTIONS_AUTOFILL_SAVE).c_str());
  g_signal_connect(G_OBJECT(form_autofill_asktosave_radio_), "toggled",
                   G_CALLBACK(OnAutofillRadioToggled), this);
  gtk_box_pack_start(GTK_BOX(vbox), form_autofill_asktosave_radio_, FALSE,
                     FALSE, 0);

  // Never save radio button.
  form_autofill_neversave_radio_ = gtk_radio_button_new_with_label_from_widget(
      GTK_RADIO_BUTTON(form_autofill_asktosave_radio_),
      l10n_util::GetStringUTF8(IDS_OPTIONS_AUTOFILL_NEVERSAVE).c_str());
  g_signal_connect(G_OBJECT(form_autofill_neversave_radio_), "toggled",
                   G_CALLBACK(OnAutofillRadioToggled), this);
  gtk_box_pack_start(GTK_BOX(vbox), form_autofill_neversave_radio_, FALSE,
                     FALSE, 0);

  return vbox;
}

GtkWidget* ContentPageGtk::InitBrowsingDataGroup() {
  GtkWidget* vbox = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);

  // Browsing data label.
  GtkWidget* browsing_data_label = gtk_label_new(
      l10n_util::GetStringUTF8(IDS_OPTIONS_BROWSING_DATA_INFO).c_str());
  gtk_label_set_line_wrap(GTK_LABEL(browsing_data_label), TRUE);
  gtk_misc_set_alignment(GTK_MISC(browsing_data_label), 0, 0);
  gtk_box_pack_start(GTK_BOX(vbox), browsing_data_label, FALSE, FALSE, 0);

  // Horizontal two button layout.
  GtkWidget* button_hbox = gtk_hbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_container_add(GTK_CONTAINER(vbox), button_hbox);

  // Import button.
  GtkWidget* import_button = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_OPTIONS_IMPORT_DATA_BUTTON).c_str());
  g_signal_connect(G_OBJECT(import_button), "clicked",
                   G_CALLBACK(OnImportButtonClicked), this);
  gtk_box_pack_start(GTK_BOX(button_hbox), import_button, FALSE, FALSE, 0);

  // Clear data button.
  GtkWidget* clear_data_button = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_OPTIONS_CLEAR_DATA_BUTTON).c_str());
  g_signal_connect(G_OBJECT(clear_data_button), "clicked",
                   G_CALLBACK(OnClearBrowsingDataButtonClicked), this);
  gtk_box_pack_start(GTK_BOX(button_hbox), clear_data_button, FALSE, FALSE, 0);

  return vbox;
}

GtkWidget* ContentPageGtk::InitThemesGroup() {
  GtkWidget* vbox = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);
  GtkWidget* hbox = gtk_hbox_new(FALSE, gtk_util::kControlSpacing);

#if defined(TOOLKIT_GTK)
  // GTK theme button.
  gtk_theme_button_ = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_THEMES_GTK_BUTTON).c_str());
  g_signal_connect(G_OBJECT(gtk_theme_button_), "clicked",
                   G_CALLBACK(OnGtkThemeButtonClicked), this);
  gtk_box_pack_start(GTK_BOX(hbox), gtk_theme_button_, FALSE, FALSE, 0);
#endif

  // Reset theme button.
  themes_reset_button_ = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_THEMES_SET_CLASSIC).c_str());
  g_signal_connect(G_OBJECT(themes_reset_button_), "clicked",
                   G_CALLBACK(OnResetDefaultThemeButtonClicked), this);
  gtk_box_pack_start(GTK_BOX(hbox), themes_reset_button_, FALSE, FALSE, 0);

  // Get themes button.
  GtkWidget* themes_gallery_button = gtk_chrome_link_button_new(
      l10n_util::GetStringUTF8(IDS_THEMES_GALLERY_BUTTON).c_str());
  g_signal_connect(G_OBJECT(themes_gallery_button), "clicked",
                   G_CALLBACK(OnGetThemesButtonClicked), this);
  gtk_box_pack_start(GTK_BOX(hbox), themes_gallery_button, FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  // "Use system title bar and borders" radio buttons.
  if (browser_defaults::kCanToggleSystemTitleBar) {
    // Use system title bar and borders
    system_title_bar_show_radio_ = gtk_radio_button_new_with_label(NULL,
        l10n_util::GetStringUTF8(IDS_SHOW_WINDOW_DECORATIONS_RADIO).c_str());
    g_signal_connect(G_OBJECT(system_title_bar_show_radio_), "toggled",
                     G_CALLBACK(OnSystemTitleBarRadioToggled), this);
    gtk_box_pack_start(GTK_BOX(vbox), system_title_bar_show_radio_, FALSE,
                       FALSE, 0);

    // Hide system title bar and use custom borders
    system_title_bar_hide_radio_ = gtk_radio_button_new_with_label_from_widget(
        GTK_RADIO_BUTTON(system_title_bar_show_radio_),
        l10n_util::GetStringUTF8(IDS_HIDE_WINDOW_DECORATIONS_RADIO).c_str());
    g_signal_connect(G_OBJECT(system_title_bar_hide_radio_), "toggled",
                     G_CALLBACK(OnSystemTitleBarRadioToggled), this);
    gtk_box_pack_start(GTK_BOX(vbox), system_title_bar_hide_radio_, FALSE,
                       FALSE, 0);
  }

  return vbox;
}

// static
void ContentPageGtk::OnImportButtonClicked(GtkButton* widget,
                                           ContentPageGtk* page) {
  ImportDialogGtk::Show(
      GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(widget))),
      page->profile());
}

// static
void ContentPageGtk::OnClearBrowsingDataButtonClicked(GtkButton* widget,
                                                      ContentPageGtk* page) {
  ClearBrowsingDataDialogGtk::Show(
      GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(widget))),
      page->profile());
}

// static
void ContentPageGtk::OnGtkThemeButtonClicked(GtkButton* widget,
                                             ContentPageGtk* page) {
  page->UserMetricsRecordAction(L"Options_GtkThemeSet",
                                page->profile()->GetPrefs());
  page->profile()->SetNativeTheme();
}

// static
void ContentPageGtk::OnResetDefaultThemeButtonClicked(GtkButton* widget,
                                                      ContentPageGtk* page) {
  page->UserMetricsRecordAction(L"Options_ThemesReset",
                                page->profile()->GetPrefs());
  page->profile()->ClearTheme();
}

// static
void ContentPageGtk::OnGetThemesButtonClicked(GtkButton* widget,
                                              ContentPageGtk* page) {
  page->UserMetricsRecordAction(L"Options_ThemesGallery",
                                page->profile()->GetPrefs());
  BrowserList::GetLastActive()->OpenThemeGalleryTabAndActivate();
}

// static
void ContentPageGtk::OnSystemTitleBarRadioToggled(GtkToggleButton* widget,
                                                  ContentPageGtk* page) {
  DCHECK(browser_defaults::kCanToggleSystemTitleBar);
  if (page->initializing_)
    return;

  // We get two signals when selecting a radio button, one for the old radio
  // being toggled off and one for the new one being toggled on. Ignore the
  // signal for the toggling off the old button.
  if (!gtk_toggle_button_get_active(widget))
    return;

  bool use_custom = gtk_toggle_button_get_active(
      GTK_TOGGLE_BUTTON(page->system_title_bar_hide_radio_));
  if (use_custom) {
    page->UserMetricsRecordAction(L"Options_CustomFrame_Enable",
                                  page->profile()->GetPrefs());
  } else {
    page->UserMetricsRecordAction(L"Options_CustomFrame_Disable",
                                  page->profile()->GetPrefs());
  }

  page->use_custom_chrome_frame_.SetValue(use_custom);
}

// static
void ContentPageGtk::OnPasswordsExceptionsButtonClicked(GtkButton* widget,
                                                        ContentPageGtk* page) {
  ShowPasswordsExceptionsWindow(page->profile());
}

// static
void ContentPageGtk::OnPasswordRadioToggled(GtkToggleButton* widget,
                                            ContentPageGtk* page) {
  if (page->initializing_)
    return;

  // We get two signals when selecting a radio button, one for the old radio
  // being toggled off and one for the new one being toggled on. Ignore the
  // signal for the toggling off the old button.
  if (!gtk_toggle_button_get_active(widget))
    return;

  bool enabled = gtk_toggle_button_get_active(
      GTK_TOGGLE_BUTTON(page->passwords_asktosave_radio_));
  if (enabled) {
    page->UserMetricsRecordAction(L"Options_PasswordManager_Enable",
                                  page->profile()->GetPrefs());
  } else {
    page->UserMetricsRecordAction(L"Options_PasswordManager_Disable",
                                  page->profile()->GetPrefs());
  }
  page->ask_to_save_passwords_.SetValue(enabled);
}

// static
void ContentPageGtk::OnAutofillRadioToggled(GtkToggleButton* widget,
                                            ContentPageGtk* page) {
  if (page->initializing_)
    return;

  // We get two signals when selecting a radio button, one for the old radio
  // being toggled off and one for the new one being toggled on. Ignore the
  // signal for the toggling off the old button.
  if (!gtk_toggle_button_get_active(widget))
    return;

  bool enabled = gtk_toggle_button_get_active(
      GTK_TOGGLE_BUTTON(page->form_autofill_asktosave_radio_));
  if (enabled) {
    page->UserMetricsRecordAction(L"Options_FormAutofill_Enable",
                                  page->profile()->GetPrefs());
  } else {
    page->UserMetricsRecordAction(L"Options_FormAutofill_Disable",
                                  page->profile()->GetPrefs());
  }
  page->ask_to_save_form_autofill_.SetValue(enabled);
}
