// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_launcher_prefs.h"

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace {

// App ID of default pinned apps.
const char* kDefaultPinnedApps[] = {
  extension_misc::kGmailAppId,
  extension_misc::kGoogleSearchAppId,
  extension_misc::kGoogleDocAppId,
  extension_misc::kYoutubeAppId,
};

base::ListValue* CreateDefaultPinnedAppsList() {
  scoped_ptr<base::ListValue> apps(new base::ListValue);
  for (size_t i = 0; i < arraysize(kDefaultPinnedApps); ++i)
    apps->Append(ash::CreateAppDict(kDefaultPinnedApps[i]));

  return apps.release();
}

}  // namespace

namespace ash {

const char kPinnedAppsPrefAppIDPath[] = "id";

const char kShelfAutoHideBehaviorAlways[] = "Always";
const char kShelfAutoHideBehaviorNever[] = "Never";

extern const char kShelfAlignmentBottom[] = "Bottom";
extern const char kShelfAlignmentLeft[] = "Left";
extern const char kShelfAlignmentRight[] = "Right";
extern const char kShelfAlignmentTop[] = "Top";

void RegisterChromeLauncherUserPrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // TODO: If we want to support multiple profiles this will likely need to be
  // pushed to local state and we'll need to track profile per item.
  registry->RegisterIntegerPref(
      prefs::kShelfChromeIconIndex,
      0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterListPref(prefs::kPinnedLauncherApps,
                             CreateDefaultPinnedAppsList(),
                             user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterStringPref(prefs::kShelfAutoHideBehavior,
                               kShelfAutoHideBehaviorNever,
                               user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kShelfAutoHideBehaviorLocal,
      std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(prefs::kShelfAlignment,
                               kShelfAlignmentBottom,
                               user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kShelfAlignmentLocal,
      std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      prefs::kShelfPreferences,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kLogoutDialogDurationMs,
      20000,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kShowLogoutButtonInTray,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

base::DictionaryValue* CreateAppDict(const std::string& app_id) {
  scoped_ptr<base::DictionaryValue> app_value(new base::DictionaryValue);
  app_value->SetString(kPinnedAppsPrefAppIDPath, app_id);
  return app_value.release();
}

}  // namespace ash
