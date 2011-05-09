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
#include "chrome/browser/sync/syncable_service.h"
#include "chrome/browser/sync/unrecoverable_error_handler.h"

class Profile;
class ProfileSyncService;
class Value;

namespace sync_api {
class WriteNode;
class WriteTransaction;
}

namespace browser_sync {
class ChangeProcessor;
class GenericChangeProcessor;
}

// Contains all model association related logic:
// * Algorithm to associate preferences model and sync model.
// TODO(sync): Rewrite to use change processor instead of transactions.
// TODO(sync): Merge this into PrefService. We don't actually need the id_map
// mapping since the sync node tags are the same as the pref names.
class PrefModelAssociator
    : public SyncableService,
      public base::NonThreadSafe {
 public:
  explicit PrefModelAssociator(PrefService* pref_service);
  virtual ~PrefModelAssociator();

  // SyncableService implementation.
  virtual bool AssociateModels() OVERRIDE;
  virtual bool DisassociateModels() OVERRIDE;
  virtual bool SyncModelHasUserCreatedNodes(bool* has_nodes) OVERRIDE;
  virtual void AbortAssociation() OVERRIDE;  // Not implemented.
  virtual bool CryptoReadyIfNecessary() OVERRIDE;
  virtual void ApplyChangesFromSync(
      const sync_api::BaseTransaction* trans,
      const sync_api::SyncManager::ChangeRecord* changes,
      int change_count) OVERRIDE;
  virtual void SetupSync(
      ProfileSyncService* sync_service,
      browser_sync::GenericChangeProcessor* change_processor) OVERRIDE;

  // Returns the list of preference names that should be monitored for changes.
  // Only preferences that are registered will be in this list.
  std::set<std::string> synced_preferences() const;

  // Register a preference with the specified name for syncing. We do not care
  // about the type at registration time, but when changes arrive from the
  // syncer, we check if they can be applied and if not drop them.
  virtual void RegisterPref(const char* name);

  // Returns true if the specified preference is registered for syncing.
  virtual bool IsPrefRegistered(const char* name);

  // Process a local preference change.
  virtual void ProcessPrefChange(const std::string& name);

  // Merges the value of local_pref into the supplied server_value and returns
  // the result (caller takes ownership). If there is a conflict, the server
  // value always takes precedence. Note that only certain preferences will
  // actually be merged, all others will return a copy of the server value. See
  // the method's implementation for details.
  static Value* MergePreference(const PrefService::Preference& local_pref,
                                const Value& server_value);

  // Writes the value of pref into the specified node. Returns true
  // upon success.
  static bool WritePreferenceToNode(const std::string& name,
                                    const Value& value,
                                    sync_api::WriteNode* node);

  // Extract preference value and name from sync specifics.
  Value* ReadPreferenceSpecifics(
      const sync_pb::PreferenceSpecifics& specifics,
      std::string* name);

  // Returns the sync id for the given preference name, or sync_api::kInvalidId
  // if the preference name is not associated to any sync id.
  int64 GetSyncIdFromChromeId(const std::string& node_id);
 protected:
  friend class ProfileSyncServicePreferenceTest;

  // For testing.
  PrefModelAssociator();

  // Create an association for a given preference. A sync node is created if
  // necessary and the value is read from or written to the node as appropriate.
  bool InitPrefNodeAndAssociate(sync_api::WriteTransaction* trans,
                                const sync_api::BaseNode& root,
                                const PrefService::Preference* pref);

  // Associates the given preference name with the given sync id.
  void Associate(const PrefService::Preference* node, int64 sync_id);

  // Remove the association that corresponds to the given sync id.
  void Disassociate(int64 sync_id);

  // Returns whether a node with the given permanent tag was found and update
  // |sync_id| with that node's id.
  bool GetSyncIdForTaggedNode(const std::string& tag, int64* sync_id);

  // Perform any additional operations that need to happen after a preference
  // has been updated.
  void SendUpdateNotificationsIfNecessary(const std::string& pref_name);

  typedef std::map<std::string, int64> PreferenceNameToSyncIdMap;
  typedef std::map<int64, std::string> SyncIdToPreferenceNameMap;

  static Value* MergeListValues(const Value& from_value, const Value& to_value);
  static Value* MergeDictionaryValues(const Value& from_value,
                                      const Value& to_value);

  PrefService* pref_service_;
  ProfileSyncService* sync_service_;

  // Do we have an active association between the preferences and sync models?
  // Set by AssociateModels, reset by DisassociateModels.
  bool models_associated_;

  // Whether we're currently processing changes from the syncer. While this is
  // true, we ignore any pref changes, since we triggered them.
  bool processing_syncer_changes_;

  PreferenceNameToSyncIdMap id_map_;
  SyncIdToPreferenceNameMap id_map_inverse_;
  std::set<std::string> synced_preferences_;

  // TODO(zea): Get rid of this and use one owned by the PSS.
  // We create, but don't own this (will be destroyed by data type controller).
  browser_sync::GenericChangeProcessor* change_processor_;

  DISALLOW_COPY_AND_ASSIGN(PrefModelAssociator);
};

#endif  // CHROME_BROWSER_PREFS_PREF_MODEL_ASSOCIATOR_H_
