// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sync_app_helper.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/extensions_helper.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_extension_helper.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/extensions/sync_helper.h"
#include "extensions/browser/app_sorting.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/id_util.h"

using extensions::ExtensionPrefs;

namespace {

struct AppState {
  AppState();
  ~AppState();
  bool IsValid() const;
  bool Equals(const AppState& other) const;

  syncer::StringOrdinal app_launch_ordinal;
  syncer::StringOrdinal page_ordinal;
  extensions::LaunchType launch_type;
  GURL launch_web_url;
  std::string description;
  std::string name;
  bool from_bookmark;
};

typedef std::map<std::string, AppState> AppStateMap;

AppState::AppState() : launch_type(extensions::LAUNCH_TYPE_INVALID) {}

AppState::~AppState() {}

bool AppState::IsValid() const {
  return page_ordinal.IsValid() && app_launch_ordinal.IsValid();
}

bool AppState::Equals(const AppState& other) const {
  return app_launch_ordinal.Equals(other.app_launch_ordinal) &&
         page_ordinal.Equals(other.page_ordinal) &&
         launch_type == other.launch_type &&
         launch_web_url == other.launch_web_url &&
         description == other.description && name == other.name &&
         from_bookmark == other.from_bookmark;
}

// Load all the app specific values for |id| into |app_state|.
void LoadApp(content::BrowserContext* context,
             const std::string& id,
             AppState* app_state) {
  ExtensionPrefs* prefs = ExtensionPrefs::Get(context);
  app_state->app_launch_ordinal = prefs->app_sorting()->GetAppLaunchOrdinal(id);
  app_state->page_ordinal = prefs->app_sorting()->GetPageOrdinal(id);
  app_state->launch_type = extensions::GetLaunchTypePrefValue(prefs, id);
  ExtensionService* service =
      extensions::ExtensionSystem::Get(context)->extension_service();
  const extensions::Extension* extension = service->GetInstalledExtension(id);
  app_state->launch_web_url =
      extensions::AppLaunchInfo::GetLaunchWebURL(extension);
  app_state->description = extension->description();
  app_state->name = extension->name();
  app_state->from_bookmark = extension->from_bookmark();
}

// Returns a map from |profile|'s installed extensions to their state.
AppStateMap GetAppStates(Profile* profile) {
  AppStateMap app_state_map;

  scoped_ptr<const extensions::ExtensionSet> extensions(
      extensions::ExtensionRegistry::Get(profile)
          ->GenerateInstalledExtensionsSet());
  for (extensions::ExtensionSet::const_iterator it = extensions->begin();
       it != extensions->end(); ++it) {
    if (extensions::sync_helper::IsSyncableApp(it->get())) {
      const std::string& id = (*it)->id();
      LoadApp(profile, id, &(app_state_map[id]));
    }
  }

  const extensions::PendingExtensionManager* pending_extension_manager =
      extensions::ExtensionSystem::Get(profile)
          ->extension_service()
          ->pending_extension_manager();

  std::list<std::string> pending_crx_ids;
  pending_extension_manager->GetPendingIdsForUpdateCheck(&pending_crx_ids);

  for (std::list<std::string>::const_iterator id = pending_crx_ids.begin();
       id != pending_crx_ids.end(); ++id) {
    LoadApp(profile, *id, &(app_state_map[*id]));
  }

  return app_state_map;
}

}  // namespace

SyncAppHelper* SyncAppHelper::GetInstance() {
  SyncAppHelper* instance = Singleton<SyncAppHelper>::get();
  instance->SetupIfNecessary(sync_datatype_helper::test());
  return instance;
}

void SyncAppHelper::SetupIfNecessary(SyncTest* test) {
  if (setup_completed_)
    return;

  for (int i = 0; i < test->num_clients(); ++i) {
    extensions::ExtensionSystem::Get(
        test->GetProfile(i))->InitForRegularProfile(true);
  }
  extensions::ExtensionSystem::Get(
      test->verifier())->InitForRegularProfile(true);

  setup_completed_ = true;
}

bool SyncAppHelper::AppStatesMatch(Profile* profile1, Profile* profile2) {
  if (!SyncExtensionHelper::GetInstance()->ExtensionStatesMatch(
          profile1, profile2))
    return false;

  const AppStateMap& state_map1 = GetAppStates(profile1);
  const AppStateMap& state_map2 = GetAppStates(profile2);
  if (state_map1.size() != state_map2.size()) {
    DVLOG(2) << "Number of Apps for profile " << profile1->GetDebugName()
             << " does not match profile " << profile2->GetDebugName();
    return false;
  }

  AppStateMap::const_iterator it1 = state_map1.begin();
  AppStateMap::const_iterator it2 = state_map2.begin();
  while (it1 != state_map1.end()) {
    if (it1->first != it2->first) {
      DVLOG(2) << "Apps for profile " << profile1->GetDebugName()
               << " do not match profile " << profile2->GetDebugName();
      return false;
    } else if (!it1->second.IsValid()) {
      DVLOG(2) << "Apps for profile " << profile1->GetDebugName()
               << " are not valid.";
      return false;
    } else if (!it2->second.IsValid()) {
      DVLOG(2) << "Apps for profile " << profile2->GetDebugName()
               << " are not valid.";
      return false;
    } else if (!it1->second.Equals(it2->second)) {
      DVLOG(2) << "App states for profile " << profile1->GetDebugName()
               << " do not match profile " << profile2->GetDebugName();
      return false;
    }
    ++it1;
    ++it2;
  }

  return true;
}

syncer::StringOrdinal SyncAppHelper::GetPageOrdinalForApp(
    Profile* profile,
    const std::string& name) {
  return ExtensionPrefs::Get(profile)->app_sorting()->GetPageOrdinal(
      extensions::id_util::GenerateId(name));
}

void SyncAppHelper::SetPageOrdinalForApp(
    Profile* profile,
    const std::string& name,
    const syncer::StringOrdinal& page_ordinal) {
  ExtensionPrefs::Get(profile)->app_sorting()->SetPageOrdinal(
      extensions::id_util::GenerateId(name), page_ordinal);
}

syncer::StringOrdinal SyncAppHelper::GetAppLaunchOrdinalForApp(
    Profile* profile,
    const std::string& name) {
  return ExtensionPrefs::Get(profile)->app_sorting()->GetAppLaunchOrdinal(
      extensions::id_util::GenerateId(name));
}

void SyncAppHelper::SetAppLaunchOrdinalForApp(
    Profile* profile,
    const std::string& name,
    const syncer::StringOrdinal& app_launch_ordinal) {
  ExtensionPrefs::Get(profile)->app_sorting()->SetAppLaunchOrdinal(
      extensions::id_util::GenerateId(name), app_launch_ordinal);
}

void SyncAppHelper::FixNTPOrdinalCollisions(Profile* profile) {
  ExtensionPrefs::Get(profile)->app_sorting()->FixNTPOrdinalCollisions();
}

SyncAppHelper::SyncAppHelper() : setup_completed_(false) {}

SyncAppHelper::~SyncAppHelper() {}
