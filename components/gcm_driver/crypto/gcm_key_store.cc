// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/crypto/gcm_key_store.h"

#include <utility>

#include "base/logging.h"
#include "components/leveldb_proto/proto_database_impl.h"
#include "crypto/curve25519.h"
#include "crypto/random.h"

namespace gcm {

// Statistics are logged to UMA with this string as part of histogram name. They
// can all be found under LevelDB.*.GCMKeyStore. Changing this needs to
// synchronize with histograms.xml, AND will also become incompatible with older
// browsers still reporting the previous values.
const char kDatabaseUMAClientName[] = "GCMKeyStore";

enum class GCMKeyStore::State {
   UNINITIALIZED,
   INITIALIZING,
   INITIALIZED,
   FAILED
};

GCMKeyStore::GCMKeyStore(
    const base::FilePath& key_store_path,
    const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner)
    : key_store_path_(key_store_path),
      blocking_task_runner_(blocking_task_runner),
      state_(State::UNINITIALIZED),
      weak_factory_(this) {
  DCHECK(blocking_task_runner);
}

GCMKeyStore::~GCMKeyStore() {}

void GCMKeyStore::GetKeys(const std::string& app_id,
                          const KeysCallback& callback) {
  LazyInitialize(base::Bind(&GCMKeyStore::GetKeysAfterInitialize,
                            weak_factory_.GetWeakPtr(), app_id, callback));
}

void GCMKeyStore::GetKeysAfterInitialize(const std::string& app_id,
                                         const KeysCallback& callback) {
  DCHECK(state_ == State::INITIALIZED || state_ == State::FAILED);
  const auto iter = key_pairs_.find(app_id);
  if (iter == key_pairs_.end() || state_ != State::INITIALIZED) {
    callback.Run(KeyPair());
    return;
  }

  callback.Run(iter->second);
}

void GCMKeyStore::CreateKeys(const std::string& app_id,
                             const KeysCallback& callback) {
  LazyInitialize(base::Bind(&GCMKeyStore::CreateKeysAfterInitialize,
                            weak_factory_.GetWeakPtr(), app_id, callback));
}

void GCMKeyStore::CreateKeysAfterInitialize(const std::string& app_id,
                                            const KeysCallback& callback) {
  DCHECK(state_ == State::INITIALIZED || state_ == State::FAILED);
  if (state_ != State::INITIALIZED) {
    callback.Run(KeyPair());
    return;
  }

  // Only allow creating new keys if no keys currently exist.
  DCHECK_EQ(0u, key_pairs_.count(app_id));

  // Create a Curve25519 private/public key-pair.
  uint8_t private_key[crypto::curve25519::kScalarBytes];
  uint8_t public_key[crypto::curve25519::kBytes];

  crypto::RandBytes(private_key, sizeof(private_key));

  // Compute the |public_key| based on the |private_key|.
  crypto::curve25519::ScalarBaseMult(private_key, public_key);

  // Store the keys in a new EncryptionData object.
  EncryptionData encryption_data;
  encryption_data.set_app_id(app_id);

  KeyPair* pair = encryption_data.add_keys();
  pair->set_type(KeyPair::ECDH_CURVE_25519);
  pair->set_private_key(private_key, sizeof(private_key));
  pair->set_public_key(public_key, sizeof(public_key));

  using EntryVectorType =
      leveldb_proto::ProtoDatabase<EncryptionData>::KeyEntryVector;

  scoped_ptr<EntryVectorType> entries_to_save(new EntryVectorType());
  scoped_ptr<std::vector<std::string>> keys_to_remove(
      new std::vector<std::string>());

  entries_to_save->push_back(std::make_pair(app_id, encryption_data));

  database_->UpdateEntries(
      entries_to_save.Pass(), keys_to_remove.Pass(),
      base::Bind(&GCMKeyStore::DidStoreKeys, weak_factory_.GetWeakPtr(), app_id,
                 *pair, callback));
}

void GCMKeyStore::DidStoreKeys(const std::string& app_id,
                               const KeyPair& pair,
                               const KeysCallback& callback,
                               bool success) {
  DCHECK_EQ(0u, key_pairs_.count(app_id));

  if (!success) {
    DVLOG(1) << "Unable to store the created key in the GCM Key Store.";
    callback.Run(KeyPair());
    return;
  }

  key_pairs_[app_id] = pair;

  callback.Run(key_pairs_[app_id]);
}

void GCMKeyStore::DeleteKeys(const std::string& app_id,
                             const DeleteCallback& callback) {
  LazyInitialize(base::Bind(&GCMKeyStore::DeleteKeysAfterInitialize,
                            weak_factory_.GetWeakPtr(), app_id, callback));
}

void GCMKeyStore::DeleteKeysAfterInitialize(const std::string& app_id,
                                            const DeleteCallback& callback) {
  DCHECK(state_ == State::INITIALIZED || state_ == State::FAILED);
  const auto iter = key_pairs_.find(app_id);
  if (iter == key_pairs_.end() || state_ != State::INITIALIZED) {
    callback.Run(true /* success */);
    return;
  }

  using EntryVectorType =
      leveldb_proto::ProtoDatabase<EncryptionData>::KeyEntryVector;

  scoped_ptr<EntryVectorType> entries_to_save(new EntryVectorType());
  scoped_ptr<std::vector<std::string>> keys_to_remove(
      new std::vector<std::string>(1, app_id));

  database_->UpdateEntries(
      entries_to_save.Pass(), keys_to_remove.Pass(),
      base::Bind(&GCMKeyStore::DidDeleteKeys, weak_factory_.GetWeakPtr(),
                 app_id, callback));
}

void GCMKeyStore::DidDeleteKeys(const std::string& app_id,
                                const DeleteCallback& callback,
                                bool success) {
  if (!success) {
    DVLOG(1) << "Unable to delete a key from the GCM Key Store.";
    callback.Run(false /* success */);
    return;
  }

  key_pairs_.erase(app_id);

  callback.Run(true /* success */);
}

void GCMKeyStore::LazyInitialize(const base::Closure& done_closure) {
  if (delayed_task_controller_.CanRunTaskWithoutDelay()) {
    done_closure.Run();
    return;
  }

  delayed_task_controller_.AddTask(done_closure);
  if (state_ == State::INITIALIZING)
    return;

  state_ = State::INITIALIZING;

  database_.reset(new leveldb_proto::ProtoDatabaseImpl<EncryptionData>(
      blocking_task_runner_));

  database_->Init(
      kDatabaseUMAClientName, key_store_path_,
      base::Bind(&GCMKeyStore::DidInitialize, weak_factory_.GetWeakPtr()));
}

void GCMKeyStore::DidInitialize(bool success) {
  if (!success) {
    DVLOG(1) << "Unable to initialize the GCM Key Store.";
    state_ = State::FAILED;

    delayed_task_controller_.SetReady();
    return;
  }

  database_->LoadEntries(
      base::Bind(&GCMKeyStore::DidLoadKeys, weak_factory_.GetWeakPtr()));
}

void GCMKeyStore::DidLoadKeys(bool success,
                              scoped_ptr<std::vector<EncryptionData>> entries) {
  if (!success) {
    DVLOG(1) << "Unable to load entries into the GCM Key Store.";
    state_ = State::FAILED;

    delayed_task_controller_.SetReady();
    return;
  }

  for (const EncryptionData& entry : *entries) {
    DCHECK_EQ(1, entry.keys_size());
    key_pairs_[entry.app_id()] = entry.keys(0);
  }

  state_ = State::INITIALIZED;

  delayed_task_controller_.SetReady();
}

}  // namespace gcm
