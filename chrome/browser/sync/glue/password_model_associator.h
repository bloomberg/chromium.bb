// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_PASSWORD_MODEL_ASSOCIATOR_H_
#define CHROME_BROWSER_SYNC_GLUE_PASSWORD_MODEL_ASSOCIATOR_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/sync/glue/data_type_error_handler.h"
#include "chrome/browser/sync/glue/model_associator.h"
#include "sync/protocol/password_specifics.pb.h"

class PasswordStore;
class ProfileSyncService;

namespace base {
class MessageLoop;
}

namespace content {
struct PasswordForm;
}

namespace syncer {
class WriteNode;
class WriteTransaction;
}

namespace browser_sync {

extern const char kPasswordTag[];

// Contains all model association related logic:
// * Algorithm to associate password model and sync model.
// * Persisting model associations and loading them back.
// We do not check if we have local data before this runs; we always
// merge and sync.
class PasswordModelAssociator
  : public PerDataTypeAssociatorInterface<std::string, std::string> {
 public:
  typedef std::vector<content::PasswordForm> PasswordVector;

  static syncer::ModelType model_type() { return syncer::PASSWORDS; }
  PasswordModelAssociator(ProfileSyncService* sync_service,
                          PasswordStore* password_store,
                          DataTypeErrorHandler* error_handler);
  virtual ~PasswordModelAssociator();

  // PerDataTypeAssociatorInterface implementation.
  //
  // Iterates through the sync model looking for matched pairs of items.
  virtual syncer::SyncError AssociateModels(
      syncer::SyncMergeResult* local_merge_result,
      syncer::SyncMergeResult* syncer_merge_result) OVERRIDE;

  // Delete all password nodes.
  bool DeleteAllNodes(syncer::WriteTransaction* trans);

  // Clears all associations.
  virtual syncer::SyncError DisassociateModels() OVERRIDE;

  // The has_nodes out param is true if the sync model has nodes other
  // than the permanent tagged nodes.
  virtual bool SyncModelHasUserCreatedNodes(bool* has_nodes) OVERRIDE;

  // See ModelAssociator interface.
  virtual void AbortAssociation() OVERRIDE;

  // See ModelAssociator interface.
  virtual bool CryptoReadyIfNecessary() OVERRIDE;

  // Not implemented.
  virtual const std::string* GetChromeNodeFromSyncId(int64 sync_id) OVERRIDE;

  // Not implemented.
  virtual bool InitSyncNodeFromChromeId(const std::string& node_id,
                                        syncer::BaseNode* sync_node) OVERRIDE;

  // Returns the sync id for the given password name, or syncer::kInvalidId
  // if the password name is not associated to any sync id.
  virtual int64 GetSyncIdFromChromeId(const std::string& node_id) OVERRIDE;

  // Associates the given password name with the given sync id.
  virtual void Associate(const std::string* node, int64 sync_id) OVERRIDE;

  // Remove the association that corresponds to the given sync id.
  virtual void Disassociate(int64 sync_id) OVERRIDE;

  // Returns whether a node with the given permanent tag was found and update
  // |sync_id| with that node's id.
  virtual bool GetSyncIdForTaggedNode(const std::string& tag, int64* sync_id);

  syncer::SyncError WriteToPasswordStore(const PasswordVector* new_passwords,
                                 const PasswordVector* updated_passwords,
                                 const PasswordVector* deleted_passwords);

  static std::string MakeTag(const content::PasswordForm& password);
  static std::string MakeTag(const sync_pb::PasswordSpecificsData& password);
  static std::string MakeTag(const std::string& origin_url,
                             const std::string& username_element,
                             const std::string& username_value,
                             const std::string& password_element,
                             const std::string& signon_realm);

  static void CopyPassword(const sync_pb::PasswordSpecificsData& password,
                           content::PasswordForm* new_password);

  static bool MergePasswords(const sync_pb::PasswordSpecificsData& password,
                             const content::PasswordForm& password_form,
                             content::PasswordForm* new_password);
  static void WriteToSyncNode(const content::PasswordForm& password_form,
                              syncer::WriteNode* node);

  // Called at various points in model association to determine if the
  // user requested an abort.
  bool IsAbortPending();

 private:
  typedef std::map<std::string, int64> PasswordToSyncIdMap;
  typedef std::map<int64, std::string> SyncIdToPasswordMap;

  ProfileSyncService* sync_service_;
  PasswordStore* password_store_;
  int64 password_node_id_;

  // Abort association pending flag and lock.  If this is set to true
  // (via the AbortAssociation method), return from the
  // AssociateModels method as soon as possible.
  base::Lock abort_association_pending_lock_;
  bool abort_association_pending_;

  base::MessageLoop* expected_loop_;

  PasswordToSyncIdMap id_map_;
  SyncIdToPasswordMap id_map_inverse_;
  DataTypeErrorHandler* error_handler_;

  DISALLOW_COPY_AND_ASSIGN(PasswordModelAssociator);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_PASSWORD_MODEL_ASSOCIATOR_H_
