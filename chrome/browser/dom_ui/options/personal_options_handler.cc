// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/options/personal_options_handler.h"

#include <string>

#include "app/l10n_util.h"
#include "base/basictypes.h"
#include "base/callback.h"
#include "base/path_service.h"
#include "base/stl_util-inl.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/dom_ui/options/dom_options_util.h"
#include "chrome/browser/dom_ui/options/options_managed_banner_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/sync_setup_flow.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/themes/browser_theme_provider.h"
#include "chrome/browser/ui/options/options_page_base.h"
#include "chrome/browser/ui/options/options_window.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "chrome/common/notification_service.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#endif  // defined(OS_CHROMEOS)
#if defined(TOOLKIT_GTK)
#include "chrome/browser/ui/gtk/gtk_theme_provider.h"
#endif  // defined(TOOLKIT_GTK)

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

  localized_strings->SetString("syncSection",
      dom_options_util::StripColon(
          l10n_util::GetStringUTF16(IDS_SYNC_OPTIONS_GROUP_NAME)));
  localized_strings->SetString("privacyDashboardLink",
      l10n_util::GetStringUTF16(IDS_SYNC_PRIVACY_DASHBOARD_LINK_LABEL));

  localized_strings->SetString("passwords",
      dom_options_util::StripColon(
          l10n_util::GetStringUTF16(IDS_OPTIONS_PASSWORDS_GROUP_NAME)));
  localized_strings->SetString("passwordsAskToSave",
      l10n_util::GetStringUTF16(IDS_OPTIONS_PASSWORDS_ASKTOSAVE));
  localized_strings->SetString("passwordsNeverSave",
      l10n_util::GetStringUTF16(IDS_OPTIONS_PASSWORDS_NEVERSAVE));
  localized_strings->SetString("manage_passwords",
      l10n_util::GetStringUTF16(IDS_OPTIONS_PASSWORDS_MANAGE_PASSWORDS));

  localized_strings->SetString("autofill",
      dom_options_util::StripColon(
          l10n_util::GetStringUTF16(IDS_AUTOFILL_SETTING_WINDOWS_GROUP_NAME)));
  localized_strings->SetString("autoFillEnabled",
      l10n_util::GetStringUTF16(IDS_OPTIONS_AUTOFILL_ENABLE));
  localized_strings->SetString("manageAutofillSettings",
      l10n_util::GetStringUTF16(IDS_OPTIONS_MANAGE_AUTOFILL_SETTINGS));

  localized_strings->SetString("browsingData",
      dom_options_util::StripColon(
          l10n_util::GetStringUTF16(IDS_OPTIONS_BROWSING_DATA_GROUP_NAME)));
  localized_strings->SetString("importData",
      l10n_util::GetStringUTF16(IDS_OPTIONS_IMPORT_DATA_BUTTON));

  localized_strings->SetString("themesGallery",
      l10n_util::GetStringUTF16(IDS_THEMES_GALLERY_BUTTON));
  localized_strings->SetString("themesGalleryURL",
      l10n_util::GetStringUTF16(IDS_THEMES_GALLERY_URL));

#if defined(TOOLKIT_GTK)
  localized_strings->SetString("appearance",
      dom_options_util::StripColon(
          l10n_util::GetStringUTF16(IDS_APPEARANCE_GROUP_NAME)));
  localized_strings->SetString("themesGTKButton",
      l10n_util::GetStringUTF16(IDS_THEMES_GTK_BUTTON));
  localized_strings->SetString("themesSetClassic",
      l10n_util::GetStringUTF16(IDS_THEMES_SET_CLASSIC));
  localized_strings->SetString("showWindowDecorations",
      l10n_util::GetStringUTF16(IDS_SHOW_WINDOW_DECORATIONS_RADIO));
  localized_strings->SetString("hideWindowDecorations",
      l10n_util::GetStringUTF16(IDS_HIDE_WINDOW_DECORATIONS_RADIO));
#else
  localized_strings->SetString("themes",
      dom_options_util::StripColon(
          l10n_util::GetStringUTF16(IDS_THEMES_GROUP_NAME)));
  localized_strings->SetString("themesReset",
      l10n_util::GetStringUTF16(IDS_THEMES_RESET_BUTTON));
#endif

  // Sync select control.
  ListValue* sync_select_list = new ListValue;
  ListValue* datatypes = new ListValue;
  datatypes->Append(Value::CreateBooleanValue(false));
  datatypes->Append(
      Value::CreateStringValue(
          l10n_util::GetStringUTF8(IDS_SYNC_OPTIONS_SELECT_DATATYPES)));
  sync_select_list->Append(datatypes);
  ListValue* everything = new ListValue;
  everything->Append(Value::CreateBooleanValue(true));
  everything->Append(
      Value::CreateStringValue(
          l10n_util::GetStringUTF8(IDS_SYNC_OPTIONS_SELECT_EVERYTHING)));
  sync_select_list->Append(everything);
  localized_strings->Set("syncSelectList", sync_select_list);

  // Sync page.
  localized_strings->SetString("syncPage",
      l10n_util::GetStringUTF16(IDS_SYNC_NTP_SYNC_SECTION_TITLE));
  localized_strings->SetString("sync_title",
      l10n_util::GetStringUTF16(IDS_CUSTOMIZE_SYNC_DESCRIPTION));
  localized_strings->SetString("syncsettings",
      l10n_util::GetStringUTF16(IDS_SYNC_DATATYPE_PREFERENCES));
  localized_strings->SetString("syncbookmarks",
      l10n_util::GetStringUTF16(IDS_SYNC_DATATYPE_BOOKMARKS));
  localized_strings->SetString("synctypedurls",
      l10n_util::GetStringUTF16(IDS_SYNC_DATATYPE_TYPED_URLS));
  localized_strings->SetString("syncpasswords",
      l10n_util::GetStringUTF16(IDS_SYNC_DATATYPE_PASSWORDS));
  localized_strings->SetString("syncextensions",
      l10n_util::GetStringUTF16(IDS_SYNC_DATATYPE_EXTENSIONS));
  localized_strings->SetString("syncautofill",
      l10n_util::GetStringUTF16(IDS_SYNC_DATATYPE_AUTOFILL));
  localized_strings->SetString("syncthemes",
      l10n_util::GetStringUTF16(IDS_SYNC_DATATYPE_THEMES));
  localized_strings->SetString("syncapps",
      l10n_util::GetStringUTF16(IDS_SYNC_DATATYPE_APPS));
  localized_strings->SetString("syncsessions",
      l10n_util::GetStringUTF16(IDS_SYNC_DATATYPE_SESSIONS));
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
  dom_ui_->RegisterMessageCallback("updatePreferredDataTypes",
      NewCallback(this, &PersonalOptionsHandler::OnPreferredDataTypesUpdated));
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
  DCHECK(service);
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

void PersonalOptionsHandler::OnLoginSuccess() {
  OnStateChanged();
}

void PersonalOptionsHandler::OnLoginFailure(
    const GoogleServiceAuthError& error) {
  OnStateChanged();
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
  FundamentalValue enabled(!is_classic_theme);
  dom_ui_->CallJavascriptFunction(
      L"options.PersonalOptions.setThemesResetButtonEnabled", enabled);
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

    DictionaryValue args;
    SyncSetupFlow::GetArgsForConfigure(sync_service, &args);

    dom_ui_->CallJavascriptFunction(
        L"PersonalOptions.setRegisteredDataTypes", args);
  } else {
    dom_ui_->CallJavascriptFunction(L"options.PersonalOptions.hideSyncSection");
  }
}

void PersonalOptionsHandler::ShowSyncLoginDialog(const ListValue* args) {
#if defined(OS_CHROMEOS)
  std::string email = chromeos::UserManager::Get()->logged_in_user().email();
  string16 message = l10n_util::GetStringFUTF16(
      IDS_SYNC_LOGIN_INTRODUCTION,
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
  dom_ui_->GetProfile()->GetBrowserSignin()->RequestSignin(
      dom_ui_->tab_contents(), UTF8ToUTF16(email), message, this);
#else
  ProfileSyncService* service = dom_ui_->GetProfile()->GetProfileSyncService();
  DCHECK(service);
  service->ShowLoginDialog(NULL);
  ProfileSyncService::SyncEvent(ProfileSyncService::START_FROM_OPTIONS);
#endif
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

void PersonalOptionsHandler::OnPreferredDataTypesUpdated(
    const ListValue* args) {
  NotificationService::current()->Notify(
      NotificationType::SYNC_DATA_TYPES_UPDATED,
      Source<Profile>(dom_ui_->GetProfile()),
      NotificationService::NoDetails());
}
