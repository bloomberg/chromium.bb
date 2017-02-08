// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_launcher_prefs.h"

#include <stddef.h>

#include <set>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/chromeos/arc/arc_support_host.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/ash/app_launcher_id.h"
#include "chrome/browser/ui/ash/launcher/launcher_controller_helper.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync/model/string_ordinal.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"

namespace ash {
namespace launcher {

namespace {

std::unique_ptr<base::DictionaryValue> CreateAppDict(
    const std::string& app_id) {
  auto app_value = base::MakeUnique<base::DictionaryValue>();
  app_value->SetString(kPinnedAppsPrefAppIDPath, app_id);
  return app_value;
}

// App ID of default pinned apps.
const char* kDefaultPinnedApps[] = {
    extension_misc::kGmailAppId, extension_misc::kGoogleDocAppId,
    extension_misc::kYoutubeAppId, ArcSupportHost::kHostAppId};

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

  // Avoid DictionaryPrefUpdate's notifications for read but unmodified prefs.
  const base::DictionaryValue* current_shelf_prefs =
      prefs->GetDictionary(prefs::kShelfPreferences);
  DCHECK(current_shelf_prefs);
  std::string display_key = base::Int64ToString(display_id);
  const base::DictionaryValue* current_display_prefs = nullptr;
  std::string current_value;
  if (current_shelf_prefs->GetDictionary(display_key, &current_display_prefs) &&
      current_display_prefs->GetString(pref_key, &current_value) &&
      current_value == value) {
    return;
  }

  DictionaryPrefUpdate update(prefs, prefs::kShelfPreferences);
  base::DictionaryValue* shelf_prefs = update.Get();
  base::DictionaryValue* display_prefs = nullptr;
  if (!shelf_prefs->GetDictionary(display_key, &display_prefs)) {
    display_prefs = new base::DictionaryValue();
    shelf_prefs->Set(display_key, display_prefs);
  }
  display_prefs->SetStringWithoutPathExpansion(pref_key, value);
}

ShelfAlignment AlignmentFromPref(const std::string& value) {
  if (value == kShelfAlignmentLeft)
    return SHELF_ALIGNMENT_LEFT;
  else if (value == kShelfAlignmentRight)
    return SHELF_ALIGNMENT_RIGHT;
  // Default to bottom.
  return SHELF_ALIGNMENT_BOTTOM;
}

const char* AlignmentToPref(ShelfAlignment alignment) {
  switch (alignment) {
    case SHELF_ALIGNMENT_BOTTOM:
      return kShelfAlignmentBottom;
    case SHELF_ALIGNMENT_LEFT:
      return kShelfAlignmentLeft;
    case SHELF_ALIGNMENT_RIGHT:
      return kShelfAlignmentRight;
    case SHELF_ALIGNMENT_BOTTOM_LOCKED:
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

bool IsAppIdArcPackage(const std::string& app_id) {
  return app_id.find('.') != app_id.npos;
}

std::vector<std::string> GetActivitiesForPackage(
    const std::string& package,
    const std::vector<std::string>& all_arc_app_ids,
    const ArcAppListPrefs& app_list_pref) {
  std::vector<std::string> activities;
  for (const std::string& app_id : all_arc_app_ids) {
    const std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
        app_list_pref.GetApp(app_id);
    if (app_info->package_name == package) {
      activities.push_back(app_info->activity);
    }
  }
  return activities;
}

// If no user-set value exists at |local_path|, the value from |synced_path| is
// copied to |local_path|.
void PropagatePrefToLocalIfNotSet(
    sync_preferences::PrefServiceSyncable* pref_service,
    const char* local_path,
    const char* synced_path) {
  if (!pref_service->FindPreference(local_path)->HasUserSetting())
    pref_service->SetString(local_path, pref_service->GetString(synced_path));
}

std::vector<AppLauncherId> AppIdsToAppLauncherIds(
    const std::vector<std::string> app_ids) {
  std::vector<AppLauncherId> app_launcher_ids(app_ids.size(), AppLauncherId());
  for (size_t i = 0; i < app_ids.size(); ++i)
    app_launcher_ids[i] = AppLauncherId(app_ids[i]);
  return app_launcher_ids;
}

struct PinInfo {
  PinInfo(const std::string& app_id, const syncer::StringOrdinal& item_ordinal)
      : app_id(app_id), item_ordinal(item_ordinal) {}

  std::string app_id;
  syncer::StringOrdinal item_ordinal;
};

struct ComparePinInfo {
  bool operator()(const PinInfo& pin1, const PinInfo& pin2) {
    return pin1.item_ordinal.LessThan(pin2.item_ordinal);
  }
};

// Helper class to keep apps in order of appearance and to provide fast way
// to check if app exists in the list.
class AppTracker {
 public:
  bool HasApp(const std::string& app_id) const {
    return app_set_.find(app_id) != app_set_.end();
  }

  void AddApp(const std::string& app_id) {
    if (HasApp(app_id))
      return;
    app_list_.push_back(app_id);
    app_set_.insert(app_id);
  }

  void MaybeAddApp(const std::string& app_id,
                   const LauncherControllerHelper* helper,
                   bool check_for_valid_app) {
    DCHECK_NE(kPinnedAppsPlaceholder, app_id);
    if (check_for_valid_app && !helper->IsValidIDForCurrentUser(app_id)) {
      return;
    }
    AddApp(app_id);
  }

  void MaybeAddAppFromPref(const base::DictionaryValue* app_pref,
                           const LauncherControllerHelper* helper,
                           bool check_for_valid_app) {
    std::string app_id;
    if (!app_pref->GetString(kPinnedAppsPrefAppIDPath, &app_id)) {
      LOG(ERROR) << "Cannot get app id from app pref entry.";
      return;
    }

    if (app_id == kPinnedAppsPlaceholder)
      return;

    bool pinned_by_policy = false;
    if (app_pref->GetBoolean(kPinnedAppsPrefPinnedByPolicy,
                             &pinned_by_policy) &&
        pinned_by_policy) {
      return;
    }

    MaybeAddApp(app_id, helper, check_for_valid_app);
  }

  const std::vector<std::string>& app_list() const { return app_list_; }

 private:
  std::vector<std::string> app_list_;
  std::set<std::string> app_set_;
};

}  // namespace

const char kPinnedAppsPrefAppIDPath[] = "id";
const char kPinnedAppsPrefPinnedByPolicy[] = "pinned_by_policy";
const char kPinnedAppsPlaceholder[] = "AppShelfIDPlaceholder--------";

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

ShelfAutoHideBehavior GetShelfAutoHideBehaviorPref(PrefService* prefs,
                                                   int64_t display_id) {
  DCHECK_NE(display_id, display::kInvalidDisplayId);

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
  DCHECK_NE(display_id, display::kInvalidDisplayId);

  const char* value = AutoHideBehaviorToPref(behavior);
  if (!value)
    return;

  SetPerDisplayPref(prefs, display_id, prefs::kShelfAutoHideBehavior, value);
  if (display_id == display::Screen::GetScreen()->GetPrimaryDisplay().id()) {
    // See comment in |kShelfAlignment| about why we have two prefs here.
    prefs->SetString(prefs::kShelfAutoHideBehaviorLocal, value);
    prefs->SetString(prefs::kShelfAutoHideBehavior, value);
  }
}

ShelfAlignment GetShelfAlignmentPref(PrefService* prefs, int64_t display_id) {
  DCHECK_NE(display_id, display::kInvalidDisplayId);

  // See comment in |kShelfAlignment| as to why we consider two prefs.
  return AlignmentFromPref(GetPerDisplayPref(
      prefs, display_id, prefs::kShelfAlignmentLocal, prefs::kShelfAlignment));
}

void SetShelfAlignmentPref(PrefService* prefs,
                           int64_t display_id,
                           ShelfAlignment alignment) {
  DCHECK_NE(display_id, display::kInvalidDisplayId);

  const char* value = AlignmentToPref(alignment);
  if (!value)
    return;

  SetPerDisplayPref(prefs, display_id, prefs::kShelfAlignment, value);
  if (display_id == display::Screen::GetScreen()->GetPrimaryDisplay().id()) {
    // See comment in |kShelfAlignment| as to why we consider two prefs.
    prefs->SetString(prefs::kShelfAlignmentLocal, value);
    prefs->SetString(prefs::kShelfAlignment, value);
  }
}

// Helper that extracts app list from policy preferences.
void GetAppsPinnedByPolicy(const PrefService* prefs,
                           const LauncherControllerHelper* helper,
                           bool check_for_valid_app,
                           AppTracker* apps) {
  DCHECK(apps);
  DCHECK(apps->app_list().empty());

  const auto* policy_apps = prefs->GetList(prefs::kPolicyPinnedLauncherApps);
  if (!policy_apps)
    return;

  // Obtain here all ids of ARC apps because it takes linear time, and getting
  // them in the loop bellow would lead to quadratic complexity.
  const ArcAppListPrefs* const arc_app_list_pref = helper->GetArcAppListPrefs();
  const std::vector<std::string> all_arc_app_ids(
      arc_app_list_pref ? arc_app_list_pref->GetAppIds()
                        : std::vector<std::string>());

  std::string app_id;
  for (size_t i = 0; i < policy_apps->GetSize(); ++i) {
    const base::DictionaryValue* dictionary = nullptr;
    if (!policy_apps->GetDictionary(i, &dictionary) ||
        !dictionary->GetString(kPinnedAppsPrefAppIDPath, &app_id)) {
      LOG(ERROR) << "Cannot extract policy app info from prefs.";
      continue;
    }
    if (IsAppIdArcPackage(app_id)) {
      if (!arc_app_list_pref)
        continue;

      // We are dealing with package name, not with 32 characters ID.
      const std::string& arc_package = app_id;
      const std::vector<std::string> activities = GetActivitiesForPackage(
          arc_package, all_arc_app_ids, *arc_app_list_pref);
      for (const auto& activity : activities) {
        const std::string arc_app_id =
            ArcAppListPrefs::GetAppId(arc_package, activity);
        apps->MaybeAddApp(arc_app_id, helper, check_for_valid_app);
      }
    } else {
      apps->MaybeAddApp(app_id, helper, check_for_valid_app);
    }
  }
}

std::vector<std::string> GetPinnedAppsFromPrefsLegacy(
    const PrefService* prefs,
    const LauncherControllerHelper* helper,
    bool check_for_valid_app) {
  // Adding the app list item to the list of items requires that the ID is not
  // a valid and known ID for the extension system. The ID was constructed that
  // way - but just to make sure...
  DCHECK(!helper->IsValidIDForCurrentUser(kPinnedAppsPlaceholder));

  const auto* pinned_apps = prefs->GetList(prefs::kPinnedLauncherApps);

  // Get the sanitized preference value for the index of the Chrome app icon.
  const size_t chrome_icon_index = std::max<size_t>(
      0, std::min<size_t>(pinned_apps->GetSize(),
                          prefs->GetInteger(prefs::kShelfChromeIconIndex)));

  // Check if Chrome is in either of the the preferences lists.
  std::unique_ptr<base::Value> chrome_app(
      CreateAppDict(extension_misc::kChromeAppId));

  AppTracker apps;
  GetAppsPinnedByPolicy(prefs, helper, check_for_valid_app, &apps);

  std::string app_id;
  for (size_t i = 0; i < pinned_apps->GetSize(); ++i) {
    // We need to position the chrome icon relative to its place in the pinned
    // preference list - even if an item of that list isn't shown yet.
    if (i == chrome_icon_index)
      apps.AddApp(extension_misc::kChromeAppId);
    const base::DictionaryValue* app_pref = nullptr;
    if (!pinned_apps->GetDictionary(i, &app_pref)) {
      LOG(ERROR) << "There is no dictionary for app entry.";
      continue;
    }
    apps.MaybeAddAppFromPref(app_pref, helper, check_for_valid_app);
  }

  // If not added yet, the chrome item will be the last item in the list.
  apps.AddApp(extension_misc::kChromeAppId);
  return apps.app_list();
}

// static
std::unique_ptr<ChromeLauncherPrefsObserver>
ChromeLauncherPrefsObserver::CreateIfNecessary(Profile* profile) {
  sync_preferences::PrefServiceSyncable* prefs =
      PrefServiceSyncableFromProfile(profile);
  if (!prefs->FindPreference(prefs::kShelfAlignmentLocal)->HasUserSetting() ||
      !prefs->FindPreference(prefs::kShelfAutoHideBehaviorLocal)
           ->HasUserSetting()) {
    return base::WrapUnique(new ChromeLauncherPrefsObserver(prefs));
  }
  return nullptr;
}

ChromeLauncherPrefsObserver::~ChromeLauncherPrefsObserver() {
  prefs_->RemoveObserver(this);
}

ChromeLauncherPrefsObserver::ChromeLauncherPrefsObserver(
    sync_preferences::PrefServiceSyncable* prefs)
    : prefs_(prefs) {
  // This causes OnIsSyncingChanged to be called when the value of
  // PrefService::IsSyncing() changes.
  prefs_->AddObserver(this);
}

void ChromeLauncherPrefsObserver::OnIsSyncingChanged() {
  // If prefs have synced, copy the values from |synced_path| to |local_path|
  // if the local values haven't already been set.
  if (!prefs_->IsSyncing())
    return;
  PropagatePrefToLocalIfNotSet(prefs_, prefs::kShelfAlignmentLocal,
                               prefs::kShelfAlignment);
  PropagatePrefToLocalIfNotSet(prefs_, prefs::kShelfAutoHideBehaviorLocal,
                               prefs::kShelfAutoHideBehavior);
  prefs_->RemoveObserver(this);
}

// Helper to create pin position that stays before any synced app, even if
// app is not currently visible on a device.
syncer::StringOrdinal GetFirstPinPosition(Profile* profile) {
  syncer::StringOrdinal position;
  app_list::AppListSyncableService* app_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile);
  for (const auto& sync_peer : app_service->sync_items()) {
    if (!sync_peer.second->item_pin_ordinal.IsValid())
      continue;
    if (!position.IsValid() ||
        sync_peer.second->item_pin_ordinal.LessThan(position)) {
      position = sync_peer.second->item_pin_ordinal;
    }
  }

  return position.IsValid() ? position.CreateBefore()
                            : syncer::StringOrdinal::CreateInitialOrdinal();
}

// Helper to creates pin position that stays before any synced app, even if
// app is not currently visible on a device.
syncer::StringOrdinal GetLastPinPosition(Profile* profile) {
  syncer::StringOrdinal position;
  app_list::AppListSyncableService* app_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile);
  for (const auto& sync_peer : app_service->sync_items()) {
    if (!sync_peer.second->item_pin_ordinal.IsValid())
      continue;
    if (!position.IsValid() ||
        sync_peer.second->item_pin_ordinal.GreaterThan(position)) {
      position = sync_peer.second->item_pin_ordinal;
    }
  }

  return position.IsValid() ? position.CreateAfter()
                            : syncer::StringOrdinal::CreateInitialOrdinal();
}

std::vector<std::string> ImportLegacyPinnedApps(
    const PrefService* prefs,
    LauncherControllerHelper* helper) {
  const std::vector<std::string> legacy_pins_all =
      GetPinnedAppsFromPrefsLegacy(prefs, helper, false);
  DCHECK(!legacy_pins_all.empty());

  app_list::AppListSyncableService* app_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(helper->profile());

  std::vector<std::string> legacy_pins_valid;
  syncer::StringOrdinal last_position =
      syncer::StringOrdinal::CreateInitialOrdinal();
  // Convert to sync item record.
  for (const auto& app_id : legacy_pins_all) {
    DCHECK_NE(kPinnedAppsPlaceholder, app_id);
    app_service->SetPinPosition(app_id, last_position);
    last_position = last_position.CreateAfter();
    if (helper->IsValidIDForCurrentUser(app_id))
      legacy_pins_valid.push_back(app_id);
  }

  // Now process default apps.
  for (size_t i = 0; i < arraysize(kDefaultPinnedApps); ++i) {
    const std::string& app_id = kDefaultPinnedApps[i];
    // Check if it is already imported.
    if (app_service->GetPinPosition(app_id).IsValid())
      continue;
    // Check if it is present but not in legacy pin.
    if (helper->IsValidIDForCurrentUser(app_id))
      continue;
    app_service->SetPinPosition(app_id, last_position);
    last_position = last_position.CreateAfter();
  }

  return legacy_pins_valid;
}

std::vector<AppLauncherId> GetPinnedAppsFromPrefs(
    const PrefService* prefs,
    LauncherControllerHelper* helper) {
  app_list::AppListSyncableService* app_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(helper->profile());
  // Some unit tests may not have it or service may not be initialized.
  if (!app_service || !app_service->IsInitialized())
    return std::vector<AppLauncherId>();

  std::vector<PinInfo> pin_infos;

  // Empty pins indicates that sync based pin model is used for the first
  // time. In normal workflow we have at least Chrome browser pin info.
  bool first_run = true;

  for (const auto& sync_peer : app_service->sync_items()) {
    if (!sync_peer.second->item_pin_ordinal.IsValid())
      continue;

    first_run = false;
    // Don't include apps that currently do not exist on device.
    if (sync_peer.first != extension_misc::kChromeAppId &&
        !helper->IsValidIDForCurrentUser(sync_peer.first)) {
      continue;
    }

    pin_infos.push_back(
        PinInfo(sync_peer.first, sync_peer.second->item_pin_ordinal));
  }

  if (first_run) {
    // Return default apps in case profile is not synced yet.
    sync_preferences::PrefServiceSyncable* const pref_service_syncable =
        PrefServiceSyncableFromProfile(helper->profile());
    if (!pref_service_syncable->IsSyncing())
      return AppIdsToAppLauncherIds(
          GetPinnedAppsFromPrefsLegacy(prefs, helper, true));

    // We need to import legacy pins model and convert it to sync based
    // model.
    return AppIdsToAppLauncherIds(ImportLegacyPinnedApps(prefs, helper));
  }

  // Sort pins according their ordinals.
  std::sort(pin_infos.begin(), pin_infos.end(), ComparePinInfo());

  AppTracker policy_apps;
  GetAppsPinnedByPolicy(prefs, helper, true, &policy_apps);

  // Pinned by policy apps appear first, if they were not shown before.
  syncer::StringOrdinal front_position = GetFirstPinPosition(helper->profile());
  std::vector<std::string>::const_reverse_iterator it;
  for (it = policy_apps.app_list().rbegin();
       it != policy_apps.app_list().rend(); ++it) {
    const std::string& app_id = *it;
    if (app_id == kPinnedAppsPlaceholder)
      continue;

    // Check if we already processed current app.
    if (app_service->GetPinPosition(app_id).IsValid())
      continue;

    // Now move it to the front.
    pin_infos.insert(pin_infos.begin(), PinInfo(*it, front_position));
    app_service->SetPinPosition(app_id, front_position);
    front_position = front_position.CreateBefore();
  }

  // Now insert Chrome browser app if needed.
  if (!app_service->GetPinPosition(extension_misc::kChromeAppId).IsValid()) {
    pin_infos.insert(pin_infos.begin(),
                     PinInfo(extension_misc::kChromeAppId, front_position));
    app_service->SetPinPosition(extension_misc::kChromeAppId, front_position);
  }

  if (helper->IsValidIDForCurrentUser(ArcSupportHost::kHostAppId)) {
    if (!app_service->GetSyncItem(ArcSupportHost::kHostAppId)) {
      const syncer::StringOrdinal arc_host_position =
          GetLastPinPosition(helper->profile());
      pin_infos.insert(pin_infos.begin(),
                       PinInfo(ArcSupportHost::kHostAppId, arc_host_position));
      app_service->SetPinPosition(ArcSupportHost::kHostAppId,
                                  arc_host_position);
    }
  }

  // Convert to AppLauncherId array.
  std::vector<std::string> pins(pin_infos.size());
  for (size_t i = 0; i < pin_infos.size(); ++i)
    pins[i] = pin_infos[i].app_id;

  return AppIdsToAppLauncherIds(pins);
}

void RemovePinPosition(Profile* profile, const AppLauncherId& app_launcher_id) {
  DCHECK(profile);

  const std::string& app_id = app_launcher_id.app_id();
  if (!app_launcher_id.launch_id().empty()) {
    VLOG(2) << "Syncing remove pin for '" << app_id
            << "' with non-empty launch id '" << app_launcher_id.launch_id()
            << "' is not supported.";
    return;
  }
  DCHECK(!app_id.empty());

  app_list::AppListSyncableService* app_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile);
  app_service->SetPinPosition(app_id, syncer::StringOrdinal());
}

void SetPinPosition(Profile* profile,
                    const AppLauncherId& app_launcher_id,
                    const AppLauncherId& app_launcher_id_before,
                    const std::vector<AppLauncherId>& app_launcher_ids_after) {
  DCHECK(profile);

  const std::string& app_id = app_launcher_id.app_id();
  if (!app_launcher_id.launch_id().empty()) {
    VLOG(2) << "Syncing set pin for '" << app_id
            << "' with non-empty launch id '" << app_launcher_id.launch_id()
            << "' is not supported.";
    return;
  }

  const std::string& app_id_before = app_launcher_id_before.app_id();

  DCHECK(!app_id.empty());
  DCHECK_NE(app_id, app_id_before);

  app_list::AppListSyncableService* app_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile);
  // Some unit tests may not have this service.
  if (!app_service)
    return;

  syncer::StringOrdinal position_before =
      app_id_before.empty() ? syncer::StringOrdinal()
                            : app_service->GetPinPosition(app_id_before);
  syncer::StringOrdinal position_after;
  for (const auto& app_launcher_id_after : app_launcher_ids_after) {
    const std::string& app_id_after = app_launcher_id_after.app_id();
    DCHECK_NE(app_id_after, app_id);
    DCHECK_NE(app_id_after, app_id_before);
    syncer::StringOrdinal position = app_service->GetPinPosition(app_id_after);
    DCHECK(position.IsValid());
    if (!position.IsValid()) {
      LOG(ERROR) << "Sync pin position was not found for " << app_id_after;
      continue;
    }
    if (!position_before.IsValid() || !position.Equals(position_before)) {
      position_after = position;
      break;
    }
  }

  syncer::StringOrdinal pin_position;
  if (position_before.IsValid() && position_after.IsValid())
    pin_position = position_before.CreateBetween(position_after);
  else if (position_before.IsValid())
    pin_position = position_before.CreateAfter();
  else if (position_after.IsValid())
    pin_position = position_after.CreateBefore();
  else
    pin_position = syncer::StringOrdinal::CreateInitialOrdinal();
  app_service->SetPinPosition(app_id, pin_position);
}

}  // namespace launcher
}  // namespace ash
