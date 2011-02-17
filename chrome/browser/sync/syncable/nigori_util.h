// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Various utility methods for nigory-based multi-type encryption.

#ifndef CHROME_BROWSER_SYNC_SYNCABLE_NIGORI_UTIL_H_
#define CHROME_BROWSER_SYNC_SYNCABLE_NIGORI_UTIL_H_
#pragma once

#include "chrome/browser/sync/protocol/nigori_specifics.pb.h"
#include "chrome/browser/sync/syncable/model_type.h"

namespace browser_sync {
class Cryptographer;
}

namespace syncable {

class BaseTransaction;
class ReadTransaction;
class WriteTransaction;

// Returns the set of datatypes that require encryption as specified by the
// Sync DB's nigori node. This will never include passwords, as the encryption
// status of that is always on if passwords are enabled..
ModelTypeSet GetEncryptedDataTypes(BaseTransaction* const trans);

// Extract the set of encrypted datatypes from a nigori node.
ModelTypeSet GetEncryptedDataTypesFromNigori(
    const sync_pb::NigoriSpecifics& nigori);

// Set the encrypted datatypes on the nigori node.
void FillNigoriEncryptedTypes(const ModelTypeSet& types,
    sync_pb::NigoriSpecifics* nigori);

// Check if our unsyced changes are encrypted if they need to be based on
// |encrypted_types|.
// Returns: true if all unsynced data that should be encrypted is.
//          false if some unsynced changes need to be encrypted.
// This method is similar to ProcessUnsyncedChangesForEncryption but does not
// modify the data and does not care if data is unnecessarily encrypted.
bool VerifyUnsyncedChangesAreEncrypted(
    BaseTransaction* const trans,
    const ModelTypeSet& encrypted_types);

// Processes all unsynced changes and ensures they are appropriately encrypted
// or unencrypted, based on |encrypted_types|.
bool ProcessUnsyncedChangesForEncryption(
    WriteTransaction* const trans,
    const syncable::ModelTypeSet& encrypted_types,
    browser_sync::Cryptographer* cryptographer);

// Verifies all data of type |type| is encrypted if |is_encrypted| is true or is
// unencrypted otherwise.
bool VerifyDataTypeEncryption(BaseTransaction* const trans,
                              ModelType type,
                              bool is_encrypted);

}  // namespace syncable

#endif  // CHROME_BROWSER_SYNC_SYNCABLE_NIGORI_UTIL_H_
