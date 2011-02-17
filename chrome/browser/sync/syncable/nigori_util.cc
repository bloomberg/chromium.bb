// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/syncable/nigori_util.h"

#include <queue>
#include <string>
#include <vector>

#include "chrome/browser/sync/engine/syncer_util.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/util/cryptographer.h"

namespace syncable {

ModelTypeSet GetEncryptedDataTypes(BaseTransaction* const trans) {
  std::string nigori_tag = ModelTypeToRootTag(syncable::NIGORI);
  Entry entry(trans, GET_BY_SERVER_TAG, nigori_tag);
  if (!entry.good()) {
    VLOG(1) << "Nigori node not found, assuming no encrypted datatypes.";
    return ModelTypeSet();
  }
  if (NIGORI != entry.GetModelType()) {
    // Can happen if we fail to apply the nigori node due to a conflict.
    VLOG(1) << "Nigori node does not have nigori extension. Assuming no"
            << " encrypted datatypes.";
    return ModelTypeSet();
  }
  const sync_pb::EntitySpecifics& specifics = entry.Get(SPECIFICS);
  return GetEncryptedDataTypesFromNigori(
      specifics.GetExtension(sync_pb::nigori));
}

ModelTypeSet GetEncryptedDataTypesFromNigori(
    const sync_pb::NigoriSpecifics& nigori) {
  // We don't check NIGORI datatype, it uses its own encryption scheme.
  ModelTypeSet encrypted_types;
  if (nigori.encrypt_bookmarks())
    encrypted_types.insert(BOOKMARKS);
  if (nigori.encrypt_preferences())
    encrypted_types.insert(PREFERENCES);
  if (nigori.encrypt_autofill_profile())
    encrypted_types.insert(AUTOFILL_PROFILE);
  if (nigori.encrypt_autofill())
    encrypted_types.insert(AUTOFILL);
  if (nigori.encrypt_themes())
    encrypted_types.insert(THEMES);
  if (nigori.encrypt_typed_urls())
    encrypted_types.insert(TYPED_URLS);
  if (nigori.encrypt_extensions())
    encrypted_types.insert(EXTENSIONS);
  if (nigori.encrypt_sessions())
    encrypted_types.insert(SESSIONS);
  if (nigori.encrypt_apps())
    encrypted_types.insert(APPS);
  return encrypted_types;
}

void FillNigoriEncryptedTypes(const ModelTypeSet& types,
    sync_pb::NigoriSpecifics* nigori) {
  DCHECK(nigori);
  nigori->set_encrypt_bookmarks(types.count(BOOKMARKS) > 0);
  nigori->set_encrypt_preferences(types.count(PREFERENCES) > 0);
  nigori->set_encrypt_autofill_profile(types.count(AUTOFILL_PROFILE) > 0);
  nigori->set_encrypt_autofill(types.count(AUTOFILL) > 0);
  nigori->set_encrypt_themes(types.count(THEMES) > 0);
  nigori->set_encrypt_typed_urls(types.count(TYPED_URLS) > 0);
  nigori->set_encrypt_extensions(types.count(EXTENSIONS) > 0);
  nigori->set_encrypt_sessions(types.count(SESSIONS) > 0);
  nigori->set_encrypt_apps(types.count(APPS) > 0);
}

bool ProcessUnsyncedChangesForEncryption(
    WriteTransaction* const trans,
    const ModelTypeSet& encrypted_types,
    browser_sync::Cryptographer* cryptographer) {
  // Get list of all datatypes with unsynced changes. It's possible that our
  // local changes need to be encrypted if encryption for that datatype was
  // just turned on (and vice versa). This should never affect passwords.
  std::vector<int64> handles;
  browser_sync::SyncerUtil::GetUnsyncedEntries(trans, &handles);
  for (size_t i = 0; i < handles.size(); ++i) {
    MutableEntry entry(trans, GET_BY_HANDLE, handles[i]);
    sync_pb::EntitySpecifics new_specifics;
    const sync_pb::EntitySpecifics& entry_specifics = entry.Get(SPECIFICS);
    ModelType type = entry.GetModelType();
    if (type == PASSWORDS)
      continue;
    if (encrypted_types.count(type) > 0 &&
        !entry_specifics.has_encrypted()) {
      // This entry now requires encryption.
      AddDefaultExtensionValue(type, &new_specifics);
      if (!cryptographer->Encrypt(
          entry_specifics,
          new_specifics.mutable_encrypted())) {
        LOG(ERROR) << "Could not encrypt data for newly encrypted type " <<
            ModelTypeToString(type);
        NOTREACHED();
        return false;
      } else {
        VLOG(1) << "Encrypted change for newly encrypted type " <<
            ModelTypeToString(type);
        entry.Put(SPECIFICS, new_specifics);
      }
    } else if (encrypted_types.count(type) == 0 &&
               entry_specifics.has_encrypted()) {
      // This entry no longer requires encryption.
      if (!cryptographer->Decrypt(entry_specifics.encrypted(),
                                  &new_specifics)) {
        LOG(ERROR) << "Could not decrypt data for newly unencrypted type " <<
            ModelTypeToString(type);
        NOTREACHED();
        return false;
      } else {
        VLOG(1) << "Decrypted change for newly unencrypted type " <<
            ModelTypeToString(type);
        entry.Put(SPECIFICS, new_specifics);
      }
    }
  }
  return true;
}

bool VerifyUnsyncedChangesAreEncrypted(
    BaseTransaction* const trans,
    const ModelTypeSet& encrypted_types) {
  std::vector<int64> handles;
  browser_sync::SyncerUtil::GetUnsyncedEntries(trans, &handles);
  for (size_t i = 0; i < handles.size(); ++i) {
    Entry entry(trans, GET_BY_HANDLE, handles[i]);
    if (!entry.good()) {
      NOTREACHED();
      return false;
    }
    const sync_pb::EntitySpecifics& entry_specifics = entry.Get(SPECIFICS);
    ModelType type = entry.GetModelType();
    if (type == PASSWORDS)
      continue;
    if (encrypted_types.count(type) > 0 &&
        !entry_specifics.has_encrypted()) {
      // This datatype requires encryption but this data is not encrypted.
      return false;
    }
  }
  return true;
}

// Mainly for testing.
bool VerifyDataTypeEncryption(BaseTransaction* const trans,
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
  Id id_string =
      trans->directory()->GetFirstChildId(trans, type_root.Get(ID));
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
      // Traverse the children.
      to_visit.push(
          trans->directory()->GetFirstChildId(trans, child.Get(ID)));
    } else {
      const sync_pb::EntitySpecifics& specifics = child.Get(SPECIFICS);
      DCHECK_EQ(type, child.GetModelType());
      DCHECK_EQ(type, GetModelTypeFromSpecifics(specifics));
      if (specifics.has_encrypted() != is_encrypted)
        return false;
    }
    // Push the successor.
    to_visit.push(child.Get(NEXT_ID));
  }
  return true;
}

}  // namespace syncable
