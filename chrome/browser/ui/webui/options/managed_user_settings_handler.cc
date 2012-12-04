// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/managed_user_settings_handler.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/common/chrome_switches.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

namespace options {

ManagedUserSettingsHandler::ManagedUserSettingsHandler() {
}

ManagedUserSettingsHandler::~ManagedUserSettingsHandler() {
}

void ManagedUserSettingsHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static OptionsStringResource resources[] = {
    // Installed content packs.
    { "installedContentPacks", IDS_INSTALLED_CONTENT_PACKS_LABEL },
    { "getContentPacks", IDS_GET_CONTENT_PACKS_BUTTON },
    { "getContentPacksURL", IDS_GET_CONTENT_PACKS_URL },
    // Content pack restriction options.
    { "contentPackSettings", IDS_CONTENT_PACK_SETTINGS_LABEL },
    { "outsideContentPacksAllow", IDS_OUTSIDE_CONTENT_PACKS_ALLOW_RADIO },
    { "outsideContentPacksWarn", IDS_OUTSIDE_CONTENT_PACKS_WARN_RADIO },
    { "outsideContentPacksBlock", IDS_OUTSIDE_CONTENT_PACKS_BLOCK_RADIO },
    // Other managed user settings
    { "advancedManagedUserSettings", IDS_ADVANCED_MANAGED_USER_LABEL },
    { "enableSafeSearch", IDS_SAFE_SEARCH_ENABLED },
    { "disableProfileSignIn", IDS_SIGNIN_SYNC_DISABLED },
    { "disableHistoryDeletion", IDS_HISTORY_DELETION_DISABLED },
    { "usePassphrase", IDS_USE_PASSPHRASE_LABEL },
    { "setPassphrase", IDS_SET_PASSPHRASE_BUTTON }
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));
  RegisterTitle(localized_strings, "managedUserSettingsPage",
                IDS_MANAGED_USER_SETTINGS_TITLE);

  localized_strings->SetBoolean(
      "managedUsersEnabled",
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableManagedUsers));
}

}  // namespace options
