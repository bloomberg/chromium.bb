// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options2/personal_options_handler2.h"

#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/user_metrics.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/options/take_photo_dialog.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/window.h"
#include "third_party/skia/include/core/SkBitmap.h"
#endif  // defined(OS_CHROMEOS)
#if defined(TOOLKIT_GTK)
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#endif  // defined(TOOLKIT_GTK)

using content::UserMetricsAction;

namespace options2 {

PersonalOptionsHandler::PersonalOptionsHandler() {
#if defined(OS_CHROMEOS)
  registrar_.Add(this,
                 chrome::NOTIFICATION_LOGIN_USER_IMAGE_CHANGED,
                 content::NotificationService::AllSources());
#endif
}

PersonalOptionsHandler::~PersonalOptionsHandler() {
}

void PersonalOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  RegisterTitle(localized_strings, "personalPage",
                IDS_OPTIONS_CONTENT_TAB_LABEL);

  localized_strings->SetString("passwords",
      l10n_util::GetStringUTF16(IDS_OPTIONS_PASSWORDS_GROUP_NAME));
  localized_strings->SetString("passwordsAskToSave",
      l10n_util::GetStringUTF16(IDS_OPTIONS_PASSWORDS_ASKTOSAVE));
  localized_strings->SetString("passwordsNeverSave",
      l10n_util::GetStringUTF16(IDS_OPTIONS_PASSWORDS_NEVERSAVE));
  localized_strings->SetString("manage_passwords",
      l10n_util::GetStringUTF16(IDS_OPTIONS_PASSWORDS_MANAGE_PASSWORDS));
#if defined(OS_MACOSX)
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (profile_manager->GetNumberOfProfiles() > 1) {
    localized_strings->SetString("macPasswordsWarning",
        l10n_util::GetStringUTF16(IDS_OPTIONS_PASSWORDS_MAC_WARNING));
  }
#endif
  localized_strings->SetString("autologinEnabled",
      l10n_util::GetStringUTF16(IDS_OPTIONS_PASSWORDS_AUTOLOGIN));

  localized_strings->SetString("autofill",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SETTING_WINDOWS_GROUP_NAME));
  localized_strings->SetString("autofillEnabled",
      l10n_util::GetStringUTF16(IDS_OPTIONS_AUTOFILL_ENABLE));
  localized_strings->SetString("manageAutofillSettings",
      l10n_util::GetStringUTF16(IDS_OPTIONS_MANAGE_AUTOFILL_SETTINGS));

  localized_strings->SetString("browsingData",
      l10n_util::GetStringUTF16(IDS_OPTIONS_BROWSING_DATA_GROUP_NAME));
  localized_strings->SetString("importData",
      l10n_util::GetStringUTF16(IDS_OPTIONS_IMPORT_DATA_BUTTON));

  localized_strings->SetString("themesGallery",
      l10n_util::GetStringUTF16(IDS_THEMES_GALLERY_BUTTON));
  localized_strings->SetString("themesGalleryURL",
      l10n_util::GetStringUTF16(IDS_THEMES_GALLERY_URL));

#if defined(TOOLKIT_GTK)
  localized_strings->SetString("appearance",
      l10n_util::GetStringUTF16(IDS_APPEARANCE_GROUP_NAME));
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
      l10n_util::GetStringUTF16(IDS_THEMES_GROUP_NAME));
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
      l10n_util::GetStringUTF16(IDS_SYNC_DATATYPE_TABS));

#if defined(OS_CHROMEOS)
  localized_strings->SetString("account",
      l10n_util::GetStringUTF16(IDS_OPTIONS_PERSONAL_ACCOUNT_GROUP_NAME));
  localized_strings->SetString("enableScreenlock",
      l10n_util::GetStringUTF16(IDS_OPTIONS_ENABLE_SCREENLOCKER_CHECKBOX));
  localized_strings->SetString("changePicture",
      l10n_util::GetStringUTF16(IDS_OPTIONS_CHANGE_PICTURE));
  if (chromeos::UserManager::Get()->user_is_logged_in()) {
    localized_strings->SetString("username",
        chromeos::UserManager::Get()->logged_in_user().email());
  }
#endif
}

void PersonalOptionsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "themesReset",
      base::Bind(&PersonalOptionsHandler::ThemesReset,
                 base::Unretained(this)));
#if defined(TOOLKIT_GTK)
  web_ui()->RegisterMessageCallback(
      "themesSetGTK",
      base::Bind(&PersonalOptionsHandler::ThemesSetGTK,
                 base::Unretained(this)));
#endif
}

void PersonalOptionsHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_BROWSER_THEME_CHANGED) {
    ObserveThemeChanged();
#if defined(OS_CHROMEOS)
  } else if (type == chrome::NOTIFICATION_LOGIN_USER_IMAGE_CHANGED) {
    UpdateAccountPicture();
#endif
  } else {
    OptionsPageUIHandler::Observe(type, source, details);
  }
}

void PersonalOptionsHandler::ObserveThemeChanged() {
  Profile* profile = Profile::FromWebUI(web_ui());
#if defined(TOOLKIT_GTK)
  GtkThemeService* theme_service = GtkThemeService::GetFrom(profile);
  bool is_gtk_theme = theme_service->UsingNativeTheme();
  base::FundamentalValue gtk_enabled(!is_gtk_theme);
  web_ui()->CallJavascriptFunction(
      "options.PersonalOptions.setGtkThemeButtonEnabled", gtk_enabled);
#else
  ThemeService* theme_service = ThemeServiceFactory::GetForProfile(profile);
  bool is_gtk_theme = false;
#endif

  bool is_classic_theme = !is_gtk_theme && theme_service->UsingDefaultTheme();
  base::FundamentalValue enabled(!is_classic_theme);
  web_ui()->CallJavascriptFunction(
      "options.PersonalOptions.setThemesResetButtonEnabled", enabled);
}

void PersonalOptionsHandler::Initialize() {
  Profile* profile = Profile::FromWebUI(web_ui());

  // Listen for theme installation.
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                 content::Source<ThemeService>(
                     ThemeServiceFactory::GetForProfile(profile)));
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_CACHED_INFO_CHANGED,
                 content::NotificationService::AllSources());
  ObserveThemeChanged();
}

void PersonalOptionsHandler::ThemesReset(const ListValue* args) {
  content::RecordAction(UserMetricsAction("Options_ThemesReset"));
  Profile* profile = Profile::FromWebUI(web_ui());
  ThemeServiceFactory::GetForProfile(profile)->UseDefaultTheme();
}

#if defined(TOOLKIT_GTK)
void PersonalOptionsHandler::ThemesSetGTK(const ListValue* args) {
  content::RecordAction(UserMetricsAction("Options_GtkThemeSet"));
  Profile* profile = Profile::FromWebUI(web_ui());
  ThemeServiceFactory::GetForProfile(profile)->SetNativeTheme();
}
#endif

#if defined(OS_CHROMEOS)
void PersonalOptionsHandler::UpdateAccountPicture() {
  std::string email = chromeos::UserManager::Get()->logged_in_user().email();
  if (!email.empty()) {
    web_ui()->CallJavascriptFunction("PersonalOptions.updateAccountPicture");
    base::StringValue email_value(email);
    web_ui()->CallJavascriptFunction("AccountsOptions.updateAccountPicture",
                                     email_value);
  }
}
#endif

}  // namespace options2
