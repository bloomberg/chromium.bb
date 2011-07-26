// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_PASSWORD_MODEL_ASSOCIATOR_H_
#define CHROME_BROWSER_SYNC_GLUE_PASSWORD_MODEL_ASSOCIATOR_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/synchronization/lock.h"
#include "base/task.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/sync/glue/model_associator.h"
#include "chrome/browser/sync/protocol/password_specifics.pb.h"

class GURL;
class MessageLoop;
class PasswordStore;
class ProfileSyncService;

namespace webkit_glue {
struct PasswordForm;
};

namespace sync_api {
class WriteNode;
class WriteTransaction;
};

namespace browser_sync {

class PasswordChangeProcessor;
class UnrecoverableErrorHandler;

extern const char kPasswordTag[];

// Contains all model association related logic:
// * Algorithm to associate password model and sync model.
// * Persisting model associations and loading them back.
// We do not check if we have local data before this runs; we always
// merge and sync.
class PasswordModelAssociator
  : public PerDataTypeAssociatorInterface<std::string, std::string> {
 public:
  typedef std::vector<webkit_glue::PasswordForm> PasswordVector;

  static syncable::ModelType model_type() { return syncable::PASSWORDS; }
  PasswordModelAssociator(ProfileSyncService* sync_service,
                          PasswordStore* password_store);
  virtual ~PasswordModelAssociator();

  // PerDataTypeAssociatorInterface implementation.
  //
  // Iterates through the sync model looking for matched pairs of items.
  virtual bool AssociateModels();

  // Delete all password nodes.
  bool DeleteAllNodes(sync_api::WriteTransaction* trans);

  // Clears all associations.
  virtual bool DisassociateModels();

  // The has_nodes out param is true if the sync model has nodes other
  // than the permanent tagged nodes.
  virtual bool SyncModelHasUserCreatedNodes(bool* has_nodes);

  // See ModelAssociator interface.
  virtual void AbortAssociation();

  // See ModelAssociator interface.
  virtual bool CryptoReadyIfNecessary();

  // Not implemented.
  virtual const std::string* GetChromeNodeFromSyncId(int64 sync_id);

  // Not implemented.
  virtual bool InitSyncNodeFromChromeId(const std::string& node_id,
                                        sync_api::BaseNode* sync_node);

  // Returns the sync id for the given password name, or sync_api::kInvalidId
  // if the password name is not associated to any sync id.
  virtual int64 GetSyncIdFromChromeId(const std::string& node_id);

  // Associates the given password name with the given sync id.
  virtual void Associate(const std::string* node, int64 sync_id);

  // Remove the association that corresponds to the given sync id.
  virtual void Disassociate(int64 sync_id);

  // Returns whether a node with the given permanent tag was found and update
  // |sync_id| with that node's id.
  virtual bool GetSyncIdForTaggedNode(const std::string& tag, int64* sync_id);

  bool WriteToPasswordStore(const PasswordVector* new_passwords,
                            const PasswordVector* updated_passwords,
                            const PasswordVector* deleted_passwords);

  static std::string MakeTag(const webkit_glue::PasswordForm& password);
  static std::string MakeTag(const sync_pb::PasswordSpecificsData& password);
  static std::string MakeTag(const std::string& origin_url,
                             const std::string& username_element,
                             const std::string& username_value,
                             const std::string& password_element,
                             const std::string& signon_realm);

  static void CopyPassword(const sync_pb::PasswordSpecificsData& password,
                           webkit_glue::PasswordForm* new_password);

  static bool MergePasswords(const sync_pb::PasswordSpecificsData& password,
                             const webkit_glue::PasswordForm& password_form,
                             webkit_glue::PasswordForm* new_password);
  static void WriteToSyncNode(const webkit_glue::PasswordForm& password_form,
                              sync_api::WriteNode* node);

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

  MessageLoop* expected_loop_;

  PasswordToSyncIdMap id_map_;
  SyncIdToPasswordMap id_map_inverse_;

  DISALLOW_COPY_AND_ASSIGN(PasswordModelAssociator);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_PASSWORD_MODEL_ASSOCIATOR_H_
