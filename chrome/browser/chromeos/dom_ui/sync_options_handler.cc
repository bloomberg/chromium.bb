// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dom_ui/sync_options_handler.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/basictypes.h"
#include "base/callback.h"
#include "base/i18n/time_formatting.h"
#include "base/stl_util-inl.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/notification_service.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"

SyncOptionsHandler::SyncOptionsHandler() {}

SyncOptionsHandler::~SyncOptionsHandler() {}

void SyncOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  // Sync page - ChromeOS
  localized_strings->SetString(L"syncPage",
      l10n_util::GetString(IDS_SYNC_NTP_SYNC_SECTION_TITLE));
  localized_strings->SetString(L"sync_title",
      l10n_util::GetString(IDS_CUSTOMIZE_SYNC_DESCRIPTION));
  localized_strings->SetString(L"syncsettings",
      l10n_util::GetString(IDS_SYNC_DATATYPE_PREFERENCES));
  localized_strings->SetString(L"syncbookmarks",
      l10n_util::GetString(IDS_SYNC_DATATYPE_BOOKMARKS));
  localized_strings->SetString(L"synctypedurls",
      l10n_util::GetString(IDS_SYNC_DATATYPE_TYPED_URLS));
  localized_strings->SetString(L"syncpasswords",
      l10n_util::GetString(IDS_SYNC_DATATYPE_PASSWORDS));
  localized_strings->SetString(L"syncextensions",
      l10n_util::GetString(IDS_SYNC_DATATYPE_EXTENSIONS));
  localized_strings->SetString(L"syncautofill",
      l10n_util::GetString(IDS_SYNC_DATATYPE_AUTOFILL));
  localized_strings->SetString(L"syncthemes",
      l10n_util::GetString(IDS_SYNC_DATATYPE_THEMES));
}
