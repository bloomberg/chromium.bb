// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sync_app_helper.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_sorting.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/extensions_helper.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_extension_helper.h"

namespace {

struct AppState {
  AppState();
  ~AppState();
  bool IsValid() const;
  bool Equals(const AppState& other) const;

  syncer::StringOrdinal app_launch_ordinal;
  syncer::StringOrdinal page_ordinal;
};

typedef std::map<std::string, AppState> AppStateMap;

AppState::AppState() {}

AppState::~AppState() {}

bool AppState::IsValid() const {
  return page_ordinal.IsValid() && app_launch_ordinal.IsValid();
}

bool AppState::Equals(const AppState& other) const {
  return app_launch_ordinal.Equals(other.app_launch_ordinal) &&
      page_ordinal.Equals(other.page_ordinal);
}

// Load all the app specific values for |id| into |app_state|.
void LoadApp(ExtensionService* extension_service,
             const std::string& id,
             AppState* app_state) {
  app_state->app_launch_ordinal = extension_service->extension_prefs()->
      extension_sorting()->GetAppLaunchOrdinal(id);
  app_state->page_ordinal = extension_service->extension_prefs()->
      extension_sorting()->GetPageOrdinal(id);
}

// Returns a map from |profile|'s installed extensions to their state.
AppStateMap GetAppStates(Profile* profile) {
  AppStateMap app_state_map;

  ExtensionService* extension_service = profile->GetExtensionService();

  scoped_ptr<const ExtensionSet> extensions(
      extension_service->GenerateInstalledExtensionsSet());
  for (ExtensionSet::const_iterator it = extensions->begin();
       it != extensions->end(); ++it) {
    if ((*it)->GetSyncType() == extensions::Extension::SYNC_TYPE_APP) {
      const std::string& id = (*it)->id();
      LoadApp(extension_service, id, &(app_state_map[id]));
    }
  }

  const extensions::PendingExtensionManager* pending_extension_manager =
      extension_service->pending_extension_manager();

  std::list<std::string> pending_crx_ids;
  pending_extension_manager->GetPendingIdsForUpdateCheck(&pending_crx_ids);

  for (std::list<std::string>::const_iterator id = pending_crx_ids.begin();
       id != pending_crx_ids.end(); ++id) {
    LoadApp(extension_service, *id, &(app_state_map[*id]));
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
  return profile->GetExtensionService()->extension_prefs()->
      extension_sorting()->GetPageOrdinal(SyncExtensionHelper::NameToId(name));
}

void SyncAppHelper::SetPageOrdinalForApp(
    Profile* profile,
    const std::string& name,
    const syncer::StringOrdinal& page_ordinal) {
  profile->GetExtensionService()->extension_prefs()->extension_sorting()->
      SetPageOrdinal(SyncExtensionHelper::NameToId(name), page_ordinal);
}

syncer::StringOrdinal SyncAppHelper::GetAppLaunchOrdinalForApp(
    Profile* profile,
    const std::string& name) {
  return profile->GetExtensionService()->extension_prefs()->
      extension_sorting()->GetAppLaunchOrdinal(
          SyncExtensionHelper::NameToId(name));
}

void SyncAppHelper::SetAppLaunchOrdinalForApp(
    Profile* profile,
    const std::string& name,
    const syncer::StringOrdinal& app_launch_ordinal) {
  profile->GetExtensionService()->extension_prefs()->extension_sorting()->
      SetAppLaunchOrdinal(SyncExtensionHelper::NameToId(name),
                          app_launch_ordinal);
}

void SyncAppHelper::FixNTPOrdinalCollisions(Profile* profile) {
  profile->GetExtensionService()->extension_prefs()->extension_sorting()->
      FixNTPOrdinalCollisions();
}

SyncAppHelper::SyncAppHelper() : setup_completed_(false) {}

SyncAppHelper::~SyncAppHelper() {}
