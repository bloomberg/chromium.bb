// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/options/sync_options_handler.h"

#include "app/l10n_util.h"
#include "base/basictypes.h"
#include "base/callback.h"
#include "base/stl_util-inl.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/sync_setup_flow.h"
#include "chrome/common/notification_service.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"

SyncOptionsHandler::SyncOptionsHandler() {}

SyncOptionsHandler::~SyncOptionsHandler() {}

bool SyncOptionsHandler::IsEnabled() {
  return ProfileSyncService::IsSyncEnabled();
}

void SyncOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  // Sync page - ChromeOS
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

void SyncOptionsHandler::Initialize() {
  ProfileSyncService* service =
      dom_ui_->GetProfile()->GetOriginalProfile()->GetProfileSyncService();
  // If service is unavailable for some good reason, 'IsEnabled()' method
  // should return false. Otherwise something is broken.
  DCHECK(service);

  DictionaryValue args;
  SyncSetupFlow::GetArgsForConfigure(service, &args);

  dom_ui_->CallJavascriptFunction(L"SyncOptions.setRegisteredDataTypes", args);
}

void SyncOptionsHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback("updatePreferredDataTypes",
      NewCallback(this, &SyncOptionsHandler::OnPreferredDataTypesUpdated));
}

void SyncOptionsHandler::OnPreferredDataTypesUpdated(const ListValue* args) {
  NotificationService::current()->Notify(
      NotificationType::SYNC_DATA_TYPES_UPDATED,
      Source<Profile>(dom_ui_->GetProfile()),
      NotificationService::NoDetails());
}
