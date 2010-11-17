// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/options/personal_options_handler.h"

#include "app/l10n_util.h"
#include "base/basictypes.h"
#include "base/callback.h"
#include "base/path_service.h"
#include "base/stl_util-inl.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/dom_ui/options/options_managed_banner_handler.h"
#if defined(TOOLKIT_GTK)
#include "chrome/browser/gtk/gtk_theme_provider.h"
#endif  // defined(TOOLKIT_GTK)
#include "chrome/browser/options_page_base.h"
#include "chrome/browser/options_window.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/themes/browser_theme_provider.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/chrome_paths.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"

PersonalOptionsHandler::PersonalOptionsHandler() {
}

PersonalOptionsHandler::~PersonalOptionsHandler() {
  ProfileSyncService* sync_service =
      dom_ui_->GetProfile()->GetProfileSyncService();
  if (sync_service)
    sync_service->RemoveObserver(this);
}

void PersonalOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  localized_strings->SetString("sync_section",
      l10n_util::GetStringUTF16(IDS_SYNC_OPTIONS_GROUP_NAME));
  localized_strings->SetString("sync_not_setup_info",
      l10n_util::GetStringFUTF16(IDS_SYNC_NOT_SET_UP_INFO,
          l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));
  localized_strings->SetString("start_sync",
      l10n_util::GetStringUTF16(IDS_SYNC_START_SYNC_BUTTON_LABEL));
  localized_strings->SetString("sync_customize",
      l10n_util::GetStringUTF16(IDS_SYNC_CUSTOMIZE_BUTTON_LABEL));
  localized_strings->SetString("stop_sync",
      l10n_util::GetStringUTF16(IDS_SYNC_STOP_SYNCING_BUTTON_LABEL));
  localized_strings->SetString("stop_syncing_explanation",
      l10n_util::GetStringFUTF16(IDS_SYNC_STOP_SYNCING_EXPLANATION_LABEL,
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));
  localized_strings->SetString("stop_syncing_title",
      l10n_util::GetStringUTF16(IDS_SYNC_STOP_SYNCING_DIALOG_TITLE));
  localized_strings->SetString("stop_syncing_confirm_button_label",
      l10n_util::GetStringUTF16(IDS_SYNC_STOP_SYNCING_CONFIRM_BUTTON_LABEL));
  localized_strings->SetString("privacy_dashboard_link",
      l10n_util::GetStringUTF16(IDS_SYNC_PRIVACY_DASHBOARD_LINK_LABEL));

  localized_strings->SetString("passwords",
      l10n_util::GetStringUTF16(IDS_OPTIONS_PASSWORDS_GROUP_NAME));
  localized_strings->SetString("passwords_asktosave",
      l10n_util::GetStringUTF16(IDS_OPTIONS_PASSWORDS_ASKTOSAVE));
  localized_strings->SetString("passwords_neversave",
      l10n_util::GetStringUTF16(IDS_OPTIONS_PASSWORDS_NEVERSAVE));
  localized_strings->SetString("showpasswords",
      l10n_util::GetStringUTF16(IDS_OPTIONS_PASSWORDS_SHOWPASSWORDS));

  localized_strings->SetString("autofill",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SETTING_WINDOWS_GROUP_NAME));
  localized_strings->SetString("autofill_options",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_OPTIONS));

  localized_strings->SetString("browsing_data",
      l10n_util::GetStringUTF16(IDS_OPTIONS_BROWSING_DATA_GROUP_NAME));
  localized_strings->SetString("import_data",
      l10n_util::GetStringUTF16(IDS_OPTIONS_IMPORT_DATA_BUTTON));

  localized_strings->SetString("themes_gallery",
      l10n_util::GetStringUTF16(IDS_THEMES_GALLERY_BUTTON));
  localized_strings->SetString("themes_gallery_url",
      l10n_util::GetStringUTF16(IDS_THEMES_GALLERY_URL));

#if defined(TOOLKIT_GTK)
  localized_strings->SetString("appearance",
      l10n_util::GetStringUTF16(IDS_APPEARANCE_GROUP_NAME));
  localized_strings->SetString("themes_GTK_button",
      l10n_util::GetStringUTF16(IDS_THEMES_GTK_BUTTON));
  localized_strings->SetString("themes_set_classic",
      l10n_util::GetStringUTF16(IDS_THEMES_SET_CLASSIC));
  localized_strings->SetString("showWindow_decorations_radio",
      l10n_util::GetStringUTF16(IDS_SHOW_WINDOW_DECORATIONS_RADIO));
  localized_strings->SetString("hideWindow_decorations_radio",
      l10n_util::GetStringUTF16(IDS_HIDE_WINDOW_DECORATIONS_RADIO));
#else
  localized_strings->SetString("themes",
      l10n_util::GetStringUTF16(IDS_THEMES_GROUP_NAME));
  localized_strings->SetString("themes_reset",
      l10n_util::GetStringUTF16(IDS_THEMES_RESET_BUTTON));
  localized_strings->SetString("themes_default",
      l10n_util::GetStringUTF16(IDS_THEMES_DEFAULT_THEME_LABEL));
#endif
}

void PersonalOptionsHandler::RegisterMessages() {
  DCHECK(dom_ui_);
  dom_ui_->RegisterMessageCallback(
      "showSyncLoginDialog",
      NewCallback(this, &PersonalOptionsHandler::ShowSyncLoginDialog));
  dom_ui_->RegisterMessageCallback(
      "themesReset",
      NewCallback(this, &PersonalOptionsHandler::ThemesReset));
#if defined(TOOLKIT_GTK)
  dom_ui_->RegisterMessageCallback(
      "themesSetGTK",
      NewCallback(this, &PersonalOptionsHandler::ThemesSetGTK));
#endif
}

void PersonalOptionsHandler::Observe(NotificationType type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  if (type == NotificationType::BROWSER_THEME_CHANGED)
    ObserveThemeChanged();
  else
    OptionsPageUIHandler::Observe(type, source, details);
}

void PersonalOptionsHandler::OnStateChanged() {
  string16 status_label;
  string16 link_label;
  ProfileSyncService* service = dom_ui_->GetProfile()->GetProfileSyncService();
  bool managed = service->IsManaged();
  bool sync_setup_completed = service->HasSyncSetupCompleted();
  bool status_has_error = sync_ui_util::GetStatusLabels(service,
      &status_label, &link_label) == sync_ui_util::SYNC_ERROR;

  string16 start_stop_button_label;
  bool is_start_stop_button_visible = false;
  bool is_start_stop_button_enabled = false;
  if (sync_setup_completed) {
    start_stop_button_label =
        l10n_util::GetStringUTF16(IDS_SYNC_STOP_SYNCING_BUTTON_LABEL);
#if defined(OS_CHROMEOS)
    is_start_stop_button_visible = false;
#else
    is_start_stop_button_visible = true;
#endif
    is_start_stop_button_enabled = !managed;
  } else if (service->SetupInProgress()) {
    start_stop_button_label =
        l10n_util::GetStringUTF16(IDS_SYNC_NTP_SETUP_IN_PROGRESS);
    is_start_stop_button_visible = true;
    is_start_stop_button_enabled = false;
  } else {
    start_stop_button_label =
        l10n_util::GetStringUTF16(IDS_SYNC_START_SYNC_BUTTON_LABEL);
    is_start_stop_button_visible = true;
    is_start_stop_button_enabled = !managed;
  }

  scoped_ptr<Value> completed(Value::CreateBooleanValue(sync_setup_completed));
  dom_ui_->CallJavascriptFunction(L"PersonalOptions.setSyncSetupCompleted",
                                  *completed);

  scoped_ptr<Value> label(Value::CreateStringValue(status_label));
  dom_ui_->CallJavascriptFunction(L"PersonalOptions.setSyncStatus",
                                  *label);

  scoped_ptr<Value> enabled(
      Value::CreateBooleanValue(is_start_stop_button_enabled));
  dom_ui_->CallJavascriptFunction(L"PersonalOptions.setStartStopButtonEnabled",
                                  *enabled);

  scoped_ptr<Value> visible(
      Value::CreateBooleanValue(is_start_stop_button_visible));
  dom_ui_->CallJavascriptFunction(L"PersonalOptions.setStartStopButtonVisible",
                                  *visible);

  label.reset(Value::CreateStringValue(start_stop_button_label));
  dom_ui_->CallJavascriptFunction(L"PersonalOptions.setStartStopButtonLabel",
                                  *label);

  visible.reset(Value::CreateBooleanValue(
      sync_setup_completed && !status_has_error));
  dom_ui_->CallJavascriptFunction(L"PersonalOptions.setCustomizeButtonVisible",
                                  *visible);

  enabled.reset(Value::CreateBooleanValue(!managed));
  dom_ui_->CallJavascriptFunction(L"PersonalOptions.setCustomizeButtonEnabled",
                                  *enabled);

  string16 customize_button_label =
    l10n_util::GetStringUTF16(IDS_SYNC_CUSTOMIZE_BUTTON_LABEL);
  label.reset(Value::CreateStringValue(customize_button_label));
  dom_ui_->CallJavascriptFunction(L"PersonalOptions.setCustomizeButtonLabel",
                                  *label);

#if !defined(OS_CHROMEOS)
  label.reset(Value::CreateStringValue(link_label));
  dom_ui_->CallJavascriptFunction(L"PersonalOptions.setSyncActionLinkLabel",
                                  *label);

  enabled.reset(Value::CreateBooleanValue(!managed));
  dom_ui_->CallJavascriptFunction(L"PersonalOptions.setSyncActionLinkEnabled",
                                  *enabled);
#endif

  visible.reset(Value::CreateBooleanValue(status_has_error));
  dom_ui_->CallJavascriptFunction(
    L"PersonalOptions.setSyncStatusErrorVisible", *visible);
#if !defined(OS_CHROMEOS)
  dom_ui_->CallJavascriptFunction(
    L"PersonalOptions.setSyncActionLinkErrorVisible", *visible);
#endif
}

void PersonalOptionsHandler::ObserveThemeChanged() {
  Profile* profile = dom_ui_->GetProfile();
#if defined(TOOLKIT_GTK)
  GtkThemeProvider* provider = GtkThemeProvider::GetFrom(profile);
  bool is_gtk_theme = provider->UseGtkTheme();
  FundamentalValue gtk_enabled(!is_gtk_theme);
  dom_ui_->CallJavascriptFunction(
      L"options.PersonalOptions.setGtkThemeButtonEnabled", gtk_enabled);
#else
  BrowserThemeProvider* provider =
      reinterpret_cast<BrowserThemeProvider*>(profile->GetThemeProvider());
  bool is_gtk_theme = false;
#endif

  bool is_classic_theme = !is_gtk_theme && provider->GetThemeID().empty();
  FundamentalValue classic_enabled(!is_classic_theme);
  dom_ui_->CallJavascriptFunction(
      L"options.PersonalOptions.setClassicThemeButtonEnabled", classic_enabled);
}

void PersonalOptionsHandler::Initialize() {
  banner_handler_.reset(
      new OptionsManagedBannerHandler(dom_ui_,
                                      ASCIIToUTF16("PersonalOptions"),
                                      OPTIONS_PAGE_CONTENT));

  // Listen for theme installation.
  registrar_.Add(this, NotificationType::BROWSER_THEME_CHANGED,
                 NotificationService::AllSources());
  ObserveThemeChanged();

  ProfileSyncService* sync_service =
      dom_ui_->GetProfile()->GetProfileSyncService();
  if (sync_service) {
    sync_service->AddObserver(this);
    OnStateChanged();
  } else {
    dom_ui_->CallJavascriptFunction(L"options.PersonalOptions.hideSyncSection");
  }
}

void PersonalOptionsHandler::ShowSyncLoginDialog(const ListValue* args) {
  ProfileSyncService* service = dom_ui_->GetProfile()->GetProfileSyncService();
  service->ShowLoginDialog(NULL);
  ProfileSyncService::SyncEvent(ProfileSyncService::START_FROM_OPTIONS);
}

void PersonalOptionsHandler::ThemesReset(const ListValue* args) {
  UserMetricsRecordAction(UserMetricsAction("Options_ThemesReset"));
  dom_ui_->GetProfile()->ClearTheme();
}

#if defined(TOOLKIT_GTK)
void PersonalOptionsHandler::ThemesSetGTK(const ListValue* args) {
  UserMetricsRecordAction(UserMetricsAction("Options_GtkThemeSet"));
  dom_ui_->GetProfile()->SetNativeTheme();
}
#endif
