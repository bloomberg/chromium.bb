// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/nigori_util.h"

#include <queue>
#include <string>
#include <vector>

#include "chrome/browser/sync/engine/syncer_util.h"
#include "chrome/browser/sync/internal_api/write_node.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/util/cryptographer.h"

namespace syncable {

bool ProcessUnsyncedChangesForEncryption(
    WriteTransaction* const trans,
    browser_sync::Cryptographer* cryptographer) {
  DCHECK(cryptographer->is_ready());
  // Get list of all datatypes with unsynced changes. It's possible that our
  // local changes need to be encrypted if encryption for that datatype was
  // just turned on (and vice versa). This should never affect passwords.
  std::vector<int64> handles;
  browser_sync::SyncerUtil::GetUnsyncedEntries(trans, &handles);
  for (size_t i = 0; i < handles.size(); ++i) {
    MutableEntry entry(trans, GET_BY_HANDLE, handles[i]);
    if (!sync_api::WriteNode::UpdateEntryWithEncryption(cryptographer,
                                                        entry.Get(SPECIFICS),
                                                        &entry)) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool VerifyUnsyncedChangesAreEncrypted(
    BaseTransaction* const trans,
    ModelEnumSet encrypted_types) {
  std::vector<int64> handles;
  browser_sync::SyncerUtil::GetUnsyncedEntries(trans, &handles);
  for (size_t i = 0; i < handles.size(); ++i) {
    Entry entry(trans, GET_BY_HANDLE, handles[i]);
    if (!entry.good()) {
      NOTREACHED();
      return false;
    }
    if (EntryNeedsEncryption(encrypted_types, entry))
      return false;
  }
  return true;
}

bool EntryNeedsEncryption(ModelEnumSet encrypted_types,
                          const Entry& entry) {
  if (!entry.Get(UNIQUE_SERVER_TAG).empty())
    return false;  // We don't encrypt unique server nodes.
  syncable::ModelType type = entry.GetModelType();
  if (type == PASSWORDS || type == NIGORI)
    return false;
  // Checking NON_UNIQUE_NAME is not necessary for the correctness of encrypting
  // the data, nor for determining if data is encrypted. We simply ensure it has
  // been overwritten to avoid any possible leaks of sensitive data.
  return SpecificsNeedsEncryption(encrypted_types, entry.Get(SPECIFICS)) ||
         (encrypted_types.Has(type) &&
          entry.Get(NON_UNIQUE_NAME) != kEncryptedString);
}

bool SpecificsNeedsEncryption(ModelEnumSet encrypted_types,
                              const sync_pb::EntitySpecifics& specifics) {
  const ModelType type = GetModelTypeFromSpecifics(specifics);
  if (type == PASSWORDS || type == NIGORI)
    return false;  // These types have their own encryption schemes.
  if (!encrypted_types.Has(type))
    return false;  // This type does not require encryption
  return !specifics.has_encrypted();
}

// Mainly for testing.
bool VerifyDataTypeEncryptionForTest(
    BaseTransaction* const trans,
    browser_sync::Cryptographer* cryptographer,
    ModelType type,
    bool is_encrypted) {
  if (type == PASSWORDS || type == NIGORI) {
    NOTREACHED();
    return true;
  }
  std::string type_tag = ModelTypeToRootTag(type);
  Entry type_root(trans, GET_BY_SERVER_TAG, type_tag);
  if (!type_root.good()) {
    NOTREACHED();
    return false;
  }

  std::queue<Id> to_visit;
  Id id_string;
  if (!trans->directory()->GetFirstChildId(
          trans, type_root.Get(ID), &id_string)) {
    NOTREACHED();
    return false;
  }
  to_visit.push(id_string);
  while (!to_visit.empty()) {
    id_string = to_visit.front();
    to_visit.pop();
    if (id_string.IsRoot())
      continue;

    Entry child(trans, GET_BY_ID, id_string);
    if (!child.good()) {
      NOTREACHED();
      return false;
    }
    if (child.Get(IS_DIR)) {
      Id child_id_string;
      if (!trans->directory()->GetFirstChildId(
              trans, child.Get(ID), &child_id_string)) {
        NOTREACHED();
        return false;
      }
      // Traverse the children.
      to_visit.push(child_id_string);
    }
    const sync_pb::EntitySpecifics& specifics = child.Get(SPECIFICS);
    DCHECK_EQ(type, child.GetModelType());
    DCHECK_EQ(type, GetModelTypeFromSpecifics(specifics));
    // We don't encrypt the server's permanent items.
    if (child.Get(UNIQUE_SERVER_TAG).empty()) {
      if (specifics.has_encrypted() != is_encrypted)
        return false;
      if (specifics.has_encrypted()) {
        if (child.Get(NON_UNIQUE_NAME) != kEncryptedString)
          return false;
        if (!cryptographer->CanDecryptUsingDefaultKey(specifics.encrypted()))
          return false;
      }
    }
    // Push the successor.
    to_visit.push(child.Get(NEXT_ID));
  }
  return true;
}

}  // namespace syncable
