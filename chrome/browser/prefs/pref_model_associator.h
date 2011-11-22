// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_PREF_MODEL_ASSOCIATOR_H_
#define CHROME_BROWSER_PREFS_PREF_MODEL_ASSOCIATOR_H_
#pragma once

#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/sync/api/syncable_service.h"
#include "chrome/browser/sync/api/sync_data.h"

namespace sync_pb {
class PreferenceSpecifics;
}

namespace base {
class Value;
}

// Contains all preference sync related logic.
// TODO(sync): Merge this into PrefService once we separate the profile
// PrefService from the local state PrefService.
class PrefModelAssociator
    : public SyncableService,
      public base::NonThreadSafe {
 public:
  PrefModelAssociator();
  virtual ~PrefModelAssociator();

  // SyncableService implementation.
  virtual SyncDataList GetAllSyncData(syncable::ModelType type) const OVERRIDE;
  virtual SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const SyncChangeList& change_list) OVERRIDE;
  virtual SyncError MergeDataAndStartSyncing(
      syncable::ModelType type,
      const SyncDataList& initial_sync_data,
      SyncChangeProcessor* sync_processor) OVERRIDE;
  virtual void StopSyncing(syncable::ModelType type) OVERRIDE;

  // Returns the list of preference names that are registered as syncable, and
  // hence should be monitored for changes.
  std::set<std::string> registered_preferences() const;

  // Register a preference with the specified name for syncing. We do not care
  // about the type at registration time, but when changes arrive from the
  // syncer, we check if they can be applied and if not drop them.
  // Note: This should only be called at profile startup time (before sync
  // begins).
  virtual void RegisterPref(const char* name);

  // Returns true if the specified preference is registered for syncing.
  virtual bool IsPrefRegistered(const char* name);

  // Process a local preference change. This can trigger new SyncChanges being
  // sent to the syncer.
  virtual void ProcessPrefChange(const std::string& name);

  void SetPrefService(PrefService* pref_service);

  // Merges the value of local_pref into the supplied server_value and returns
  // the result (caller takes ownership). If there is a conflict, the server
  // value always takes precedence. Note that only certain preferences will
  // actually be merged, all others will return a copy of the server value. See
  // the method's implementation for details.
  static base::Value* MergePreference(
      const PrefService::Preference& local_pref,
      const base::Value& server_value);

  // Fills |sync_data| with a sync representation of the preference data
  // provided.
  static bool CreatePrefSyncData(const std::string& name,
                                 const base::Value& value,
                                 SyncData* sync_data);

  // Extract preference value and name from sync specifics.
  base::Value* ReadPreferenceSpecifics(
      const sync_pb::PreferenceSpecifics& specifics,
      std::string* name);

 protected:
  friend class ProfileSyncServicePreferenceTest;

  typedef std::map<std::string, SyncData> SyncDataMap;

  // Create an association for a given preference. If |sync_pref| is valid,
  // signifying that sync has data for this preference, we reconcile their data
  // with ours and append a new UPDATE SyncChange to |sync_changes|. If
  // sync_pref is not set, we append an ADD SyncChange to |sync_changes| with
  // the current preference data.
  // Note: We do not modify the sync data for preferences that are either
  // controlled by policy (are not user modifiable) or have their default value
  // (are not user controlled).
  void InitPrefAndAssociate(const SyncData& sync_pref,
                            const std::string& pref_name,
                            SyncChangeList* sync_changes);

  static base::Value* MergeListValues(
      const base::Value& from_value, const base::Value& to_value);
  static base::Value* MergeDictionaryValues(const base::Value& from_value,
                                            const base::Value& to_value);

  // Do we have an active association between the preferences and sync models?
  // Set when start syncing, reset in StopSyncing. While this is not set, we
  // ignore any local preference changes (when we start syncing we will look
  // up the most recent values anyways).
  bool models_associated_;

  // Whether we're currently processing changes from the syncer. While this is
  // true, we ignore any local preference changes, since we triggered them.
  bool processing_syncer_changes_;

  // A set of preference names.
  typedef std::set<std::string> PreferenceSet;

  // All preferences that have registered as being syncable with this profile.
  PreferenceSet registered_preferences_;

  // The preferences that are currently synced (excludes those preferences
  // that have never had sync data and currently have default values or are
  // policy controlled).
  // Note: this set never decreases, only grows to eventually match
  // registered_preferences_ as more preferences are synced. It determines
  // whether a preference change should update an existing sync node or create
  // a new sync node.
  PreferenceSet synced_preferences_;

  // The PrefService we are syncing to.
  PrefService* pref_service_;

  // Sync's SyncChange handler. We push all our changes through this.
  SyncChangeProcessor* sync_processor_;

  DISALLOW_COPY_AND_ASSIGN(PrefModelAssociator);
};

#endif  // CHROME_BROWSER_PREFS_PREF_MODEL_ASSOCIATOR_H_
