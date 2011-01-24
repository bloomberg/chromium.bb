// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_AUTOFILL_MODEL_ASSOCIATOR_H_
#define CHROME_BROWSER_SYNC_GLUE_AUTOFILL_MODEL_ASSOCIATOR_H_
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

class AutofillChangeProcessor;
class UnrecoverableErrorHandler;

extern const char kAutofillTag[];
extern const char kAutofillProfileNamespaceTag[];
extern const char kAutofillEntryNamespaceTag[];

// Contains all model association related logic:
// * Algorithm to associate autofill model and sync model.
// We do not check if we have local data before this run; we always
// merge and sync.
class AutofillModelAssociator
    : public PerDataTypeAssociatorInterface<std::string, std::string> {
 public:
  static syncable::ModelType model_type() { return syncable::AUTOFILL; }
  AutofillModelAssociator(ProfileSyncService* sync_service,
                          WebDatabase* web_database,
                          PersonalDataManager* data_manager);
  virtual ~AutofillModelAssociator();

  // PerDataTypeAssociatorInterface implementation.
  //
  // Iterates through the sync model looking for matched pairs of items.
  virtual bool AssociateModels();

  // Clears all associations.
  virtual bool DisassociateModels();

  // The has_nodes out param is true if the sync model has nodes other
  // than the permanent tagged nodes.
  virtual bool SyncModelHasUserCreatedNodes(bool* has_nodes);

  // See ModelAssociator interface.
  virtual void AbortAssociation();

  // Not implemented.
  virtual const std::string* GetChromeNodeFromSyncId(int64 sync_id);

  // Not implemented.
  virtual bool InitSyncNodeFromChromeId(const std::string& node_id,
                                        sync_api::BaseNode* sync_node);

  // Returns the sync id for the given autofill name, or sync_api::kInvalidId
  // if the autofill name is not associated to any sync id.
  virtual int64 GetSyncIdFromChromeId(const std::string& node_id);

  // Associates the given autofill name with the given sync id.
  virtual void Associate(const std::string* node, int64 sync_id);

  // Remove the association that corresponds to the given sync id.
  virtual void Disassociate(int64 sync_id);

  // Returns whether a node with the given permanent tag was found and update
  // |sync_id| with that node's id.
  virtual bool GetSyncIdForTaggedNode(const std::string& tag, int64* sync_id);

  static std::string KeyToTag(const string16& name, const string16& value);

  static bool MergeTimestamps(const sync_pb::AutofillSpecifics& autofill,
                              const std::vector<base::Time>& timestamps,
                              std::vector<base::Time>* new_timestamps);
  static bool FillProfileWithServerData(
      AutoFillProfile* merge_into,
      const sync_pb::AutofillProfileSpecifics& specifics);

  // TODO(georgey) : add the same processing for CC info (already in protocol
  // buffers).

  // Is called to determine if we need to upgrade to the new
  // autofillprofile2 data type. If so we need to sync up autofillprofile
  // first to the latest available changes on the server and then upgrade
  // to autofillprofile2.
  virtual bool HasNotMigratedYet(const sync_api::BaseTransaction* trans);

 protected:
  // Given a profile from sync db it tries to match the profile against
  // one in web db. it ignores the guid and compares the actual data.
  AutoFillProfile* FindCorrespondingNodeFromWebDB(
      const sync_pb::AutofillProfileSpecifics& profile,
      const std::vector<AutoFillProfile*>& all_profiles_from_db);

 private:
  typedef std::map<std::string, int64> AutofillToSyncIdMap;
  typedef std::map<int64, std::string> SyncIdToAutofillMap;

  // A convenience wrapper of a bunch of state we pass around while associating
  // models, and send to the WebDatabase for persistence.
  struct DataBundle;

  // Helper to query WebDatabase for the current autofill state.
  bool LoadAutofillData(std::vector<AutofillEntry>* entries,
                        std::vector<AutoFillProfile*>* profiles);

  // We split up model association first by autofill sub-type (entries, and
  // profiles.  There is a Traverse* method for each of these.
  bool TraverseAndAssociateChromeAutofillEntries(
      sync_api::WriteTransaction* write_trans,
      const sync_api::ReadNode& autofill_root,
      const std::vector<AutofillEntry>& all_entries_from_db,
      std::set<AutofillKey>* current_entries,
      std::vector<AutofillEntry>* new_entries);
  bool TraverseAndAssociateChromeAutoFillProfiles(
      sync_api::WriteTransaction* write_trans,
      const sync_api::ReadNode& autofill_root,
      const std::vector<AutoFillProfile*>& all_profiles_from_db,
      std::set<string16>* current_profiles,
      std::vector<AutoFillProfile*>* updated_profiles);

  // Once the above traversals are complete, we traverse the sync model to
  // associate all remaining nodes.
  bool TraverseAndAssociateAllSyncNodes(
      sync_api::WriteTransaction* write_trans,
      const sync_api::ReadNode& autofill_root,
      DataBundle* bundle,
      const std::vector<AutoFillProfile*>& all_profiles_from_db);

  // Helper to persist any changes that occured during model association to
  // the WebDatabase.
  bool SaveChangesToWebData(const DataBundle& bundle);

  // Helper to insert an AutofillEntry into the WebDatabase (e.g. in response
  // to encountering a sync node that doesn't exist yet locally).
  void AddNativeEntryIfNeeded(const sync_pb::AutofillSpecifics& autofill,
                              DataBundle* bundle,
                              const sync_api::ReadNode& node);

  // Helper to insert an AutoFillProfile into the WebDatabase (e.g. in response
  // to encountering a sync node that doesn't exist yet locally).
  void AddNativeProfileIfNeeded(
      const sync_pb::AutofillProfileSpecifics& profile,
      DataBundle* bundle,
      const sync_api::ReadNode& node,
      const std::vector<AutoFillProfile*>& all_profiles_from_db);

  // Called at various points in model association to determine if the
  // user requested an abort.
  bool IsAbortPending();

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
  int number_of_entries_created_;

  DISALLOW_COPY_AND_ASSIGN(AutofillModelAssociator);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_AUTOFILL_MODEL_ASSOCIATOR_H_
