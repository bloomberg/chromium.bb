// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_launcher_prefs.h"

#include <stddef.h>

#include <memory>

#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "ui/gfx/screen.h"

namespace ash {

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
    apps->Append(CreateAppDict(kDefaultPinnedApps[i]));

  return apps.release();
}

// Returns the preference value for the display with the given |display_id|.
// The pref value is stored in |local_path| and |path|, but the pref service may
// have per-display preferences and the value can be specified by policy.
// Here is the priority:
//  * A value managed by policy. This is a single value that applies to all
//    displays.
//  * A user-set value for the specified display.
//  * A user-set value in |local_path| or |path|, if no per-display settings are
//    ever specified (see http://crbug.com/173719 for why). |local_path| is
//    preferred. See comment in |kShelfAlignment| as to why we consider two
//    prefs and why |local_path| is preferred.
//  * A value recommended by policy. This is a single value that applies to all
//    root windows.
//  * The default value for |local_path| if the value is not recommended by
//    policy.
std::string GetPerDisplayPref(PrefService* prefs,
                              int64_t display_id,
                              const char* local_path,
                              const char* path) {
  const PrefService::Preference* local_pref = prefs->FindPreference(local_path);
  const std::string value(prefs->GetString(local_path));
  if (local_pref->IsManaged())
    return value;

  std::string pref_key = base::Int64ToString(display_id);
  bool has_per_display_prefs = false;
  if (!pref_key.empty()) {
    const base::DictionaryValue* shelf_prefs =
        prefs->GetDictionary(prefs::kShelfPreferences);
    const base::DictionaryValue* display_pref = nullptr;
    std::string per_display_value;
    if (shelf_prefs->GetDictionary(pref_key, &display_pref) &&
        display_pref->GetString(path, &per_display_value))
      return per_display_value;

    // If the pref for the specified display is not found, scan the whole prefs
    // and check if the prefs for other display is already specified.
    std::string unused_value;
    for (base::DictionaryValue::Iterator iter(*shelf_prefs); !iter.IsAtEnd();
         iter.Advance()) {
      const base::DictionaryValue* display_pref = nullptr;
      if (iter.value().GetAsDictionary(&display_pref) &&
          display_pref->GetString(path, &unused_value)) {
        has_per_display_prefs = true;
        break;
      }
    }
  }

  if (local_pref->IsRecommended() || !has_per_display_prefs)
    return value;

  const base::Value* default_value = prefs->GetDefaultPrefValue(local_path);
  std::string default_string;
  default_value->GetAsString(&default_string);
  return default_string;
}

// Sets the preference value for the display with the given |display_id|.
void SetPerDisplayPref(PrefService* prefs,
                       int64_t display_id,
                       const char* pref_key,
                       const std::string& value) {
  if (display_id < 0)
    return;

  DictionaryPrefUpdate update(prefs, prefs::kShelfPreferences);
  base::DictionaryValue* shelf_prefs = update.Get();
  base::DictionaryValue* pref_dictionary = nullptr;
  std::string key = base::Int64ToString(display_id);
  if (!shelf_prefs->GetDictionary(key, &pref_dictionary)) {
    pref_dictionary = new base::DictionaryValue();
    shelf_prefs->Set(key, pref_dictionary);
  }
  pref_dictionary->SetStringWithoutPathExpansion(pref_key, value);
}

wm::ShelfAlignment AlignmentFromPref(const std::string& value) {
  if (value == kShelfAlignmentLeft)
    return wm::SHELF_ALIGNMENT_LEFT;
  else if (value == kShelfAlignmentRight)
    return wm::SHELF_ALIGNMENT_RIGHT;
  // Default to bottom.
  return wm::SHELF_ALIGNMENT_BOTTOM;
}

const char* AlignmentToPref(wm::ShelfAlignment alignment) {
  switch (alignment) {
    case wm::SHELF_ALIGNMENT_BOTTOM:
      return kShelfAlignmentBottom;
    case wm::SHELF_ALIGNMENT_LEFT:
      return kShelfAlignmentLeft;
    case wm::SHELF_ALIGNMENT_RIGHT:
      return kShelfAlignmentRight;
    case wm::SHELF_ALIGNMENT_BOTTOM_LOCKED:
      // This should not be a valid preference option for now. We only want to
      // lock the shelf during login or when adding a user.
      return nullptr;
  }
  NOTREACHED();
  return nullptr;
}

ShelfAutoHideBehavior AutoHideBehaviorFromPref(const std::string& value) {
  // Note: To maintain sync compatibility with old images of chrome/chromeos
  // the set of values that may be encountered includes the now-extinct
  // "Default" as well as "Never" and "Always", "Default" should now
  // be treated as "Never" (http://crbug.com/146773).
  if (value == kShelfAutoHideBehaviorAlways)
    return SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS;
  return SHELF_AUTO_HIDE_BEHAVIOR_NEVER;
}

const char* AutoHideBehaviorToPref(ShelfAutoHideBehavior behavior) {
  switch (behavior) {
    case SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS:
      return kShelfAutoHideBehaviorAlways;
    case SHELF_AUTO_HIDE_BEHAVIOR_NEVER:
      return kShelfAutoHideBehaviorNever;
    case SHELF_AUTO_HIDE_ALWAYS_HIDDEN:
      // This should not be a valid preference option for now. We only want to
      // completely hide it when we run in app mode - or while we temporarily
      // hide the shelf as part of an animation (e.g. the multi user change).
      return nullptr;
  }
  NOTREACHED();
  return nullptr;
}

}  // namespace

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

ShelfAutoHideBehavior GetShelfAutoHideBehaviorPref(PrefService* prefs,
                                                   int64_t display_id) {
  DCHECK_GE(display_id, 0);

  // Don't show the shelf in app mode.
  if (chrome::IsRunningInAppMode())
    return SHELF_AUTO_HIDE_ALWAYS_HIDDEN;

  // See comment in |kShelfAlignment| as to why we consider two prefs.
  return AutoHideBehaviorFromPref(
      GetPerDisplayPref(prefs, display_id, prefs::kShelfAutoHideBehaviorLocal,
                        prefs::kShelfAutoHideBehavior));
}

void SetShelfAutoHideBehaviorPref(PrefService* prefs,
                                  int64_t display_id,
                                  ShelfAutoHideBehavior behavior) {
  DCHECK_GE(display_id, 0);

  const char* value = AutoHideBehaviorToPref(behavior);
  if (!value)
    return;

  SetPerDisplayPref(prefs, display_id, prefs::kShelfAutoHideBehavior, value);
  if (display_id == gfx::Screen::GetScreen()->GetPrimaryDisplay().id()) {
    // See comment in |kShelfAlignment| about why we have two prefs here.
    prefs->SetString(prefs::kShelfAutoHideBehaviorLocal, value);
    prefs->SetString(prefs::kShelfAutoHideBehavior, value);
  }
}

wm::ShelfAlignment GetShelfAlignmentPref(PrefService* prefs,
                                         int64_t display_id) {
  DCHECK_GE(display_id, 0);

  // See comment in |kShelfAlignment| as to why we consider two prefs.
  return AlignmentFromPref(GetPerDisplayPref(
      prefs, display_id, prefs::kShelfAlignmentLocal, prefs::kShelfAlignment));
}

void SetShelfAlignmentPref(PrefService* prefs,
                           int64_t display_id,
                           wm::ShelfAlignment alignment) {
  DCHECK_GE(display_id, 0);

  const char* value = AlignmentToPref(alignment);
  if (!value)
    return;

  SetPerDisplayPref(prefs, display_id, prefs::kShelfAlignment, value);
  if (display_id == gfx::Screen::GetScreen()->GetPrimaryDisplay().id()) {
    // See comment in |kShelfAlignment| as to why we consider two prefs.
    prefs->SetString(prefs::kShelfAlignmentLocal, value);
    prefs->SetString(prefs::kShelfAlignment, value);
  }
}

}  // namespace ash
