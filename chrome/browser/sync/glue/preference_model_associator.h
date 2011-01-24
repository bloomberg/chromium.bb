// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_PREFERENCE_MODEL_ASSOCIATOR_H_
#define CHROME_BROWSER_SYNC_GLUE_PREFERENCE_MODEL_ASSOCIATOR_H_
#pragma once

#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/sync/glue/model_associator.h"
#include "chrome/browser/sync/unrecoverable_error_handler.h"

class ProfileSyncService;
class Value;

namespace sync_api {
class WriteNode;
class WriteTransaction;
}

namespace browser_sync {

class PreferenceChangeProcessor;

static const char kPreferencesTag[] = "google_chrome_preferences";

// Contains all model association related logic:
// * Algorithm to associate preferences model and sync model.
class PreferenceModelAssociator
    : public PerDataTypeAssociatorInterface<PrefService::Preference,
                                            std::string> {
 public:
  static syncable::ModelType model_type() { return syncable::PREFERENCES; }
  explicit PreferenceModelAssociator(ProfileSyncService* sync_service);
  virtual ~PreferenceModelAssociator();

  // Returns the list of preference names that should be monitored for
  // changes.  Only preferences that are registered will be in this
  // list.
  const std::set<std::string>& synced_preferences() {
    return synced_preferences_;
  }

  // Create an association for a given preference. A sync node is created if
  // necessary and the value is read from or written to the node as appropriate.
  bool InitPrefNodeAndAssociate(sync_api::WriteTransaction* trans,
                                const sync_api::BaseNode& root,
                                const PrefService::Preference* pref);

  // PerDataTypeAssociatorInterface implementation.
  //
  // Iterates through the sync model looking for matched pairs of items.
  virtual bool AssociateModels();

  // Clears all associations.
  virtual bool DisassociateModels();

  // Returns whether the sync model has nodes other than the permanent tagged
  // nodes.
  virtual bool SyncModelHasUserCreatedNodes(bool* has_nodes);

  virtual void AbortAssociation() {
    // No implementation needed, this associator runs on the main
    // thread.
  }

  // Not implemented.
  virtual const PrefService::Preference* GetChromeNodeFromSyncId(int64 sync_id);

  // Not implemented.
  virtual bool InitSyncNodeFromChromeId(const std::string& node_id,
                                        sync_api::BaseNode* sync_node);

  // Returns the sync id for the given preference name, or sync_api::kInvalidId
  // if the preference name is not associated to any sync id.
  virtual int64 GetSyncIdFromChromeId(const std::string& node_id);

  // Associates the given preference name with the given sync id.
  virtual void Associate(const PrefService::Preference* node, int64 sync_id);

  // Remove the association that corresponds to the given sync id.
  virtual void Disassociate(int64 sync_id);

  // Returns whether a node with the given permanent tag was found and update
  // |sync_id| with that node's id.
  virtual bool GetSyncIdForTaggedNode(const std::string& tag, int64* sync_id);

  // Merges the value of local_pref into the supplied server_value and
  // returns the result (caller takes ownership).  If there is a
  // conflict, the server value always takes precedence.  Note that
  // only certain preferences will actually be merged, all others will
  // return a copy of the server value.  See the method's
  // implementation for details.
  static Value* MergePreference(const PrefService::Preference& local_pref,
                                const Value& server_value);

  // Writes the value of pref into the specified node.  Returns true
  // upon success.
  static bool WritePreferenceToNode(const std::string& name,
                                    const Value& value,
                                    sync_api::WriteNode* node);

  // Perform any additional operations that need to happen after a preference
  // has been updated.
  void AfterUpdateOperations(const std::string& pref_name);

 private:
  typedef std::map<std::string, int64> PreferenceNameToSyncIdMap;
  typedef std::map<int64, std::string> SyncIdToPreferenceNameMap;

  static Value* MergeListValues(const Value& from_value, const Value& to_value);
  static Value* MergeDictionaryValues(const Value& from_value,
                                      const Value& to_value);

  ProfileSyncService* sync_service_;
  std::set<std::string> synced_preferences_;
  int64 preferences_node_id_;

  PreferenceNameToSyncIdMap id_map_;
  SyncIdToPreferenceNameMap id_map_inverse_;

  DISALLOW_COPY_AND_ASSIGN(PreferenceModelAssociator);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_PREFERENCE_MODEL_ASSOCIATOR_H_
