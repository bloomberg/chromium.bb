// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_AUTOFILL_PROFILE_MODEL_ASSOCIATOR_H_
#define CHROME_BROWSER_SYNC_GLUE_AUTOFILL_PROFILE_MODEL_ASSOCIATOR_H_
#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/model_associator.h"
#include "chrome/browser/sync/protocol/autofill_specifics.pb.h"
#include "chrome/browser/webdata/autofill_entry.h"

class AutoFillProfile;

class ProfileSyncService;
class WebDatabase;

namespace sync_api {
class WriteTransaction;
}

namespace browser_sync {

extern const char kAutofillProfileTag[];

class AutofillChangeProcessor;
class UnrecoverableErrorHandler;

// Contains all model association related logic:
// * Algorithm to associate autofill model and sync model.
// We do not check if we have local data before this run; we always
// merge and sync.
class AutofillProfileModelAssociator
    : public PerDataTypeAssociatorInterface<std::string, std::string> {
 public:
  AutofillProfileModelAssociator(ProfileSyncService* sync_service,
                                 WebDatabase* web_database,
                                 PersonalDataManager* data_manager);
  virtual ~AutofillProfileModelAssociator();

  // A convenience wrapper of a bunch of state we pass around while
  // associating models, and send to the WebDatabase for persistence.
  // We do this so we hold the write lock for only a small period.
  // When storing the web db we are out of the write lock.
  struct DataBundle;

  static syncable::ModelType model_type() { return syncable::AUTOFILL_PROFILE; }

  // PerDataTypeAssociatorInterface implementation.
  //
  // Iterates through the sync model looking for matched pairs of items.
  virtual bool AssociateModels();

  // Clears all associations.
  virtual bool DisassociateModels();

  // TODO(lipalani) Bug 64111.
  // The has_nodes out param is true if the sync model has nodes other
  // than the permanent tagged nodes.
  virtual bool SyncModelHasUserCreatedNodes(bool* has_nodes);

  // See ModelAssociator interface.
  virtual void AbortAssociation();

  virtual const std::string* GetChromeNodeFromSyncId(int64 sync_id);

  virtual bool InitSyncNodeFromChromeId(const std::string& node_id,
                                        sync_api::BaseNode* sync_node);

  // Returns the sync id for the given autofill name, or sync_api::kInvalidId
  // if the autofill name is not associated to any sync id.
  virtual int64 GetSyncIdFromChromeId(const std::string& node_id);

  // Associates the given autofill name with the given sync id.
  virtual void Associate(const std::string* node, int64 sync_id);

  // Remove the association that corresponds to the given sync id.
  virtual void Disassociate(int64 sync_id);

  // TODO(lipalani) Bug 64111. Returns whether a node with the
  // given permanent tag was found and update
  // |sync_id| with that node's id. No current use. To Implement
  // only for completeness.
  virtual bool GetSyncIdForTaggedNode(const std::string& tag, int64* sync_id);

  static bool OverwriteProfileWithServerData(
      AutoFillProfile* merge_into,
      const sync_pb::AutofillProfileSpecifics& specifics);

 protected:
  AutofillProfileModelAssociator();
  bool TraverseAndAssociateChromeAutoFillProfiles(
      sync_api::WriteTransaction* write_trans,
      const sync_api::ReadNode& autofill_root,
      const std::vector<AutoFillProfile*>& all_profiles_from_db,
      std::set<std::string>* current_profiles,
      std::vector<AutoFillProfile*>* updated_profiles,
      std::vector<AutoFillProfile*>* new_profiles,
      std::vector<std::string>* profiles_to_delete);

  // Helper to insert an AutoFillProfile into the WebDatabase (e.g. in response
  // to encountering a sync node that doesn't exist yet locally).
  virtual void AddNativeProfileIfNeeded(
      const sync_pb::AutofillProfileSpecifics& profile,
      DataBundle* bundle,
      const sync_api::ReadNode& node);

  // Helper to insert a sync node for the given AutoFillProfile (e.g. in
  // response to encountering a native profile that doesn't exist yet in the
  // cloud).
  virtual bool MakeNewAutofillProfileSyncNodeIfNeeded(
      sync_api::WriteTransaction* trans,
      const sync_api::BaseNode& autofill_root,
      const AutoFillProfile& profile,
      std::vector<AutoFillProfile*>* new_profiles,
      std::set<std::string>* current_profiles,
      std::vector<std::string>* profiles_to_delete);

  // Once the above traversals are complete, we traverse the sync model to
  // associate all remaining nodes.
  bool TraverseAndAssociateAllSyncNodes(
      sync_api::WriteTransaction* write_trans,
      const sync_api::ReadNode& autofill_root,
      DataBundle* bundle);

 private:
  typedef std::map<std::string, int64> AutofillToSyncIdMap;
  typedef std::map<int64, std::string> SyncIdToAutofillMap;

  // A convenience wrapper of a bunch of state we pass around while associating
  // models, and send to the WebDatabase for persistence.
  // struct DataBundle;

  // Helper to query WebDatabase for the current autofill state.
  bool LoadAutofillData(std::vector<AutoFillProfile*>* profiles);

  static bool MergeField(FormGroup* f,
                         AutofillFieldType t,
                         const std::string& specifics_field);

  // Helper to persist any changes that occured during model association to
  // the WebDatabase.
  bool SaveChangesToWebData(const DataBundle& bundle);

  // Called at various points in model association to determine if the
  // user requested an abort.
  bool IsAbortPending();

  int64 FindSyncNodeWithProfile(sync_api::WriteTransaction* trans,
      const sync_api::BaseNode& autofill_root,
      const AutoFillProfile& profile,
      std::set<std::string>* current_profiles);

  ProfileSyncService* sync_service_;
  WebDatabase* web_database_;
  PersonalDataManager* personal_data_;
  int64 autofill_node_id_;

  AutofillToSyncIdMap id_map_;
  SyncIdToAutofillMap id_map_inverse_;

  // Abort association pending flag and lock.  If this is set to true
  // (via the AbortAssociation method), return from the
  // AssociateModels method as soon as possible.
  base::Lock abort_association_pending_lock_;
  bool abort_association_pending_;

  int number_of_profiles_created_;

  DISALLOW_COPY_AND_ASSIGN(AutofillProfileModelAssociator);
};

struct AutofillProfileModelAssociator::DataBundle {
  DataBundle();
  ~DataBundle();

  std::set<std::string> current_profiles;
  std::vector<std::string> profiles_to_delete;
  std::vector<AutoFillProfile*> updated_profiles;
  std::vector<AutoFillProfile*> new_profiles;  // We own these pointers.
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_AUTOFILL_PROFILE_MODEL_ASSOCIATOR_H_

