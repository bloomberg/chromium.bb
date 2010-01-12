// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_PREFERENCE_MODEL_ASSOCIATOR_H_
#define CHROME_BROWSER_SYNC_GLUE_PREFERENCE_MODEL_ASSOCIATOR_H_

#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "chrome/browser/sync/glue/model_associator.h"
#include "chrome/common/pref_service.h"

class ProfileSyncService;

namespace browser_sync {

class PreferenceChangeProcessor;

static const char kPreferencesTag[] = "google_chrome_preferences";

// Contains all model association related logic:
// * Algorithm to associate preferences model and sync model.
// * Persisting model associations and loading them back.
class PreferenceModelAssociator
    : public PerDataTypeAssociatorInterface<Preference, std::wstring> {
 public:
  explicit PreferenceModelAssociator(ProfileSyncService* sync_service);
  virtual ~PreferenceModelAssociator() { }

  // Returns the list of preference names that should be monitored for changes.
  const std::set<std::wstring>& synced_preferences() {
    return synced_preferences_;
  }

  // PerDataTypeAssociatorInterface implementation.
  //
  // Iterates through the sync model looking for matched pairs of items.
  virtual bool AssociateModels();

  // Clears all associations.
  virtual bool DisassociateModels();

  // Returns whether the sync model has nodes other than the permanent tagged
  // nodes.
  virtual bool SyncModelHasUserCreatedNodes();

  // Returns whether the preference model has any user-defined preferences.
  virtual bool ChromeModelHasUserCreatedNodes();

  // Not implemented.
  virtual const Preference* GetChromeNodeFromSyncId(int64 sync_id) {
    return NULL;
  }

  // Not implemented.
  virtual bool InitSyncNodeFromChromeId(std::wstring node_id,
                                        sync_api::BaseNode* sync_node) {
    return false;
  }

  // Returns the sync id for the given preference name, or sync_api::kInvalidId
  // if the preference name is not associated to any sync id.
  virtual int64 GetSyncIdForChromeId(std::wstring node_id);

  // Associates the given preference name with the given sync id.
  virtual void Associate(std::wstring node_id, int64 sync_id);

  // Remove the association that corresponds to the given sync id.
  virtual void Disassociate(int64 sync_id);

  // Returns whether a node with the given permanent tag was found and update
  // |sync_id| with that node's id.
  bool GetSyncIdForTaggedNode(const std::string& tag, int64* sync_id);

 protected:
  // Returns sync service instance.
  ProfileSyncService* sync_service() { return sync_service_; }

 private:
  typedef std::map<std::wstring, int64> PreferenceNameToSyncIdMap;
  typedef std::map<int64, std::wstring> SyncIdToPreferenceNameMap;
  typedef std::set<int64> DirtyAssociationsSyncIds;

  // Posts a task to persist dirty associations.
  void PostPersistAssociationsTask();

  // Persists all dirty associations.
  void PersistAssociations();

  ProfileSyncService* sync_service_;
  std::set<std::wstring> synced_preferences_;
  int64 preferences_node_id_;

  PreferenceNameToSyncIdMap id_map_;
  SyncIdToPreferenceNameMap id_map_inverse_;
  // Stores the IDs of dirty associations.
  DirtyAssociationsSyncIds dirty_associations_sync_ids_;

  // Used to post PersistAssociation tasks to the current message loop and
  // guarantees no invocations can occur if |this| has been deleted. (This
  // allows this class to be non-refcounted).
  ScopedRunnableMethodFactory<PreferenceModelAssociator> persist_associations_;

  DISALLOW_COPY_AND_ASSIGN(PreferenceModelAssociator);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_PREFERENCE_MODEL_ASSOCIATOR_H_
