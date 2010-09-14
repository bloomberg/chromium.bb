// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/personal_options_handler.h"

#include "app/l10n_util.h"
#include "base/basictypes.h"
#include "base/callback.h"
#include "base/path_service.h"
#include "base/stl_util-inl.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/dom_ui/options_managed_banner_handler.h"
#include "chrome/browser/options_page_base.h"
#include "chrome/browser/options_window.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/browser/sync/profile_sync_service.h"
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
}

void PersonalOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {

  DCHECK(localized_strings);
  // Personal Stuff page
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
  localized_strings->SetString("themes_gallery",
      l10n_util::GetStringUTF16(IDS_THEMES_GALLERY_BUTTON));
#else
  localized_strings->SetString("themes",
      l10n_util::GetStringUTF16(IDS_THEMES_GROUP_NAME));
  localized_strings->SetString("themes_reset",
      l10n_util::GetStringUTF16(IDS_THEMES_RESET_BUTTON));
  localized_strings->SetString("themes_gallery",
      l10n_util::GetStringUTF16(IDS_THEMES_GALLERY_BUTTON));
  localized_strings->SetString("themes_default",
      l10n_util::GetStringUTF16(IDS_THEMES_DEFAULT_THEME_LABEL));
#endif
}

void PersonalOptionsHandler::RegisterMessages() {
  DCHECK(dom_ui_);
  dom_ui_->RegisterMessageCallback(
      "getSyncStatus",
      NewCallback(this, &PersonalOptionsHandler::SetSyncStatusUIString));
  dom_ui_->RegisterMessageCallback(
      "themesReset",
      NewCallback(this, &PersonalOptionsHandler::ThemesReset));
  dom_ui_->RegisterMessageCallback(
      "themesGallery",
      NewCallback(this, &PersonalOptionsHandler::ThemesGallery));
#if defined(TOOLKIT_GTK)
  dom_ui_->RegisterMessageCallback(
      "themesSetGTK",
      NewCallback(this, &PersonalOptionsHandler::ThemesSetGTK));
#endif
}

void PersonalOptionsHandler::Initialize() {
  banner_handler_.reset(
      new OptionsManagedBannerHandler(dom_ui_,
                                      ASCIIToUTF16("PersonalOptions"),
                                      OPTIONS_PAGE_CONTENT));
}

void PersonalOptionsHandler::SetSyncStatusUIString(const ListValue* args) {
  DCHECK(dom_ui_);

  ProfileSyncService* service = dom_ui_->GetProfile()->GetProfileSyncService();
  if (service != NULL && ProfileSyncService::IsSyncEnabled()) {
    scoped_ptr<Value> status_string(Value::CreateStringValue(
        l10n_util::GetStringFUTF16(IDS_SYNC_ACCOUNT_SYNCED_TO_USER_WITH_TIME,
                                   service->GetAuthenticatedUsername(),
                                   service->GetLastSyncedTimeString())));

    dom_ui_->CallJavascriptFunction(
        L"PersonalOptions.syncStatusCallback",
        *(status_string.get()));
  }
}

void PersonalOptionsHandler::ThemesReset(const ListValue* args) {
  DCHECK(dom_ui_);

  UserMetricsRecordAction(UserMetricsAction("Options_ThemesReset"),
                          dom_ui_->GetProfile()->GetPrefs());
  dom_ui_->GetProfile()->ClearTheme();
}

void PersonalOptionsHandler::ThemesGallery(const ListValue* args) {
  DCHECK(dom_ui_);

  UserMetricsRecordAction(UserMetricsAction("Options_ThemesGallery"),
                          dom_ui_->GetProfile()->GetPrefs());
  BrowserList::GetLastActive()->OpenThemeGalleryTabAndActivate();
}

#if defined(TOOLKIT_GTK)
void PersonalOptionsHandler::ThemesSetGTK(const ListValue* args) {
  DCHECK(dom_ui_);

  UserMetricsRecordAction(UserMetricsAction("Options_GtkThemeSet"),
                          dom_ui_->GetProfile()->GetPrefs());
  dom_ui_->GetProfile()->SetNativeTheme();
}
#endif
