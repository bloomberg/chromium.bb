// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_launcher_prefs.h"

#include <stddef.h>

#include <memory>

#include "base/macros.h"
#include "base/values.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace {

// App ID of default pinned apps.
const char* kDefaultPinnedApps[] = {
  extension_misc::kGmailAppId,
  extension_misc::kGoogleDocAppId,
  extension_misc::kYoutubeAppId,
};

base::ListValue* CreateDefaultPinnedAppsList() {
  std::unique_ptr<base::ListValue> apps(new base::ListValue);
  for (size_t i = 0; i < arraysize(kDefaultPinnedApps); ++i)
    apps->Append(ash::CreateAppDict(kDefaultPinnedApps[i]));

  return apps.release();
}

}  // namespace

namespace ash {

const char kPinnedAppsPrefAppIDPath[] = "id";
const char kPinnedAppsPrefPinnedByPolicy[] = "pinned_by_policy";

const char kShelfAutoHideBehaviorAlways[] = "Always";
const char kShelfAutoHideBehaviorNever[] = "Never";

const char kShelfAlignmentBottom[] = "Bottom";
const char kShelfAlignmentLeft[] = "Left";
const char kShelfAlignmentRight[] = "Right";

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
  registry->RegisterListPref(prefs::kPolicyPinnedLauncherApps);
  registry->RegisterStringPref(prefs::kShelfAutoHideBehavior,
                               kShelfAutoHideBehaviorNever,
                               user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterStringPref(prefs::kShelfAutoHideBehaviorLocal,
                               std::string());
  registry->RegisterStringPref(prefs::kShelfAlignment,
                               kShelfAlignmentBottom,
                               user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterStringPref(prefs::kShelfAlignmentLocal, std::string());
  registry->RegisterDictionaryPref(prefs::kShelfPreferences);
  registry->RegisterIntegerPref(prefs::kLogoutDialogDurationMs, 20000);
  registry->RegisterBooleanPref(prefs::kShowLogoutButtonInTray, false);
}

base::DictionaryValue* CreateAppDict(const std::string& app_id) {
  std::unique_ptr<base::DictionaryValue> app_value(new base::DictionaryValue);
  app_value->SetString(kPinnedAppsPrefAppIDPath, app_id);
  return app_value.release();
}

ash::ShelfAlignment AlignmentFromPref(const std::string& value) {
  if (value == ash::kShelfAlignmentLeft)
    return ash::SHELF_ALIGNMENT_LEFT;
  else if (value == ash::kShelfAlignmentRight)
    return ash::SHELF_ALIGNMENT_RIGHT;
  // Default to bottom.
  return ash::SHELF_ALIGNMENT_BOTTOM;
}

const char* AlignmentToPref(ash::ShelfAlignment alignment) {
  switch (alignment) {
    case ash::SHELF_ALIGNMENT_BOTTOM:
      return ash::kShelfAlignmentBottom;
    case ash::SHELF_ALIGNMENT_LEFT:
      return ash::kShelfAlignmentLeft;
    case ash::SHELF_ALIGNMENT_RIGHT:
      return ash::kShelfAlignmentRight;
  }
  NOTREACHED();
  return nullptr;
}

ash::ShelfAutoHideBehavior AutoHideBehaviorFromPref(const std::string& value) {
  // Note: To maintain sync compatibility with old images of chrome/chromeos
  // the set of values that may be encountered includes the now-extinct
  // "Default" as well as "Never" and "Always", "Default" should now
  // be treated as "Never" (http://crbug.com/146773).
  if (value == ash::kShelfAutoHideBehaviorAlways)
    return ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS;
  return ash::SHELF_AUTO_HIDE_BEHAVIOR_NEVER;
}

const char* AutoHideBehaviorToPref(ash::ShelfAutoHideBehavior behavior) {
  switch (behavior) {
    case ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS:
      return ash::kShelfAutoHideBehaviorAlways;
    case ash::SHELF_AUTO_HIDE_BEHAVIOR_NEVER:
      return ash::kShelfAutoHideBehaviorNever;
    case ash::SHELF_AUTO_HIDE_ALWAYS_HIDDEN:
      // This one should not be a valid preference option for now. We only want
      // to completely hide it when we run in app mode - or while we temporarily
      // hide the shelf as part of an animation (e.g. the multi user change).
      return nullptr;
  }
  NOTREACHED();
  return nullptr;
}

}  // namespace ash
