// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Various utility methods for nigori-based multi-type encryption.

#ifndef CHROME_BROWSER_SYNC_ENGINE_NIGORI_UTIL_H_
#define CHROME_BROWSER_SYNC_ENGINE_NIGORI_UTIL_H_
#pragma once

#include "chrome/browser/sync/protocol/nigori_specifics.pb.h"
#include "chrome/browser/sync/syncable/model_type.h"

namespace browser_sync {
class Cryptographer;
}

namespace sync_pb {
class EntitySpecifics;
}

namespace syncable {

const char kEncryptedString[] = "encrypted";

class BaseTransaction;
class Entry;
class ReadTransaction;
class WriteTransaction;

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
    browser_sync::Cryptographer* cryptographer);

// Returns true if the entry requires encryption but is not encrypted, false
// otherwise. Note: this does not check that already encrypted entries are
// encrypted with the proper key.
bool EntryNeedsEncryption(const ModelTypeSet& encrypted_types,
                          const Entry& entry);

// Same as EntryNeedsEncryption, but looks at specifics.
bool SpecificsNeedsEncryption(const ModelTypeSet& encrypted_types,
                              const sync_pb::EntitySpecifics& specifics);

// Verifies all data of type |type| is encrypted appropriately.
bool VerifyDataTypeEncryption(BaseTransaction* const trans,
                              browser_sync::Cryptographer* cryptographer,
                              ModelType type,
                              bool is_encrypted);

}  // namespace syncable

#endif  // CHROME_BROWSER_SYNC_ENGINE_NIGORI_UTIL_H_
