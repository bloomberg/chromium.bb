// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/crypto/gcm_key_store.h"

#include <stddef.h>

#include <utility>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "components/gcm_driver/crypto/p256_key_util.h"
#include "components/leveldb_proto/proto_database_impl.h"
#include "crypto/random.h"

namespace gcm {

// Statistics are logged to UMA with this string as part of histogram name. They
// can all be found under LevelDB.*.GCMKeyStore. Changing this needs to
// synchronize with histograms.xml, AND will also become incompatible with older
// browsers still reporting the previous values.
const char kDatabaseUMAClientName[] = "GCMKeyStore";

// Number of cryptographically secure random bytes to generate as a key pair's
// authentication secret. Must be at least 16 bytes.
const size_t kAuthSecretBytes = 16;

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
  const auto& iter = key_pairs_.find(app_id);

  const bool success = state_ == State::INITIALIZED && iter != key_pairs_.end();
  UMA_HISTOGRAM_BOOLEAN("GCM.Crypto.GetKeySuccessRate", success);

  if (!success) {
    callback.Run(KeyPair(), std::string() /* auth_secret */);
    return;
  }

  const auto& auth_secret_iter = auth_secrets_.find(app_id);
  DCHECK(auth_secret_iter != auth_secrets_.end());

  callback.Run(iter->second, auth_secret_iter->second);
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
    callback.Run(KeyPair(), std::string() /* auth_secret */);
    return;
  }

  // Only allow creating new keys if no keys currently exist.
  DCHECK_EQ(0u, key_pairs_.count(app_id));

  std::string private_key, public_key_x509, public_key;
  if (!CreateP256KeyPair(&private_key, &public_key_x509, &public_key)) {
    NOTREACHED() << "Unable to initialize a P-256 key pair.";

    callback.Run(KeyPair(), std::string() /* auth_secret */);
    return;
  }

  std::string auth_secret;

  // Create the authentication secret, which has to be a cryptographically
  // secure random number of at least 128 bits (16 bytes).
  crypto::RandBytes(base::WriteInto(&auth_secret, kAuthSecretBytes + 1),
                    kAuthSecretBytes);

  // Store the keys in a new EncryptionData object.
  EncryptionData encryption_data;
  encryption_data.set_app_id(app_id);
  encryption_data.set_auth_secret(auth_secret);

  KeyPair* pair = encryption_data.add_keys();
  pair->set_type(KeyPair::ECDH_P256);
  pair->set_private_key(private_key);
  pair->set_public_key_x509(public_key_x509);
  pair->set_public_key(public_key);

  using EntryVectorType =
      leveldb_proto::ProtoDatabase<EncryptionData>::KeyEntryVector;

  std::unique_ptr<EntryVectorType> entries_to_save(new EntryVectorType());
  std::unique_ptr<std::vector<std::string>> keys_to_remove(
      new std::vector<std::string>());

  entries_to_save->push_back(std::make_pair(app_id, encryption_data));

  database_->UpdateEntries(
      std::move(entries_to_save), std::move(keys_to_remove),
      base::Bind(&GCMKeyStore::DidStoreKeys, weak_factory_.GetWeakPtr(), app_id,
                 *pair, auth_secret, callback));
}

void GCMKeyStore::DidStoreKeys(const std::string& app_id,
                               const KeyPair& pair,
                               const std::string& auth_secret,
                               const KeysCallback& callback,
                               bool success) {
  UMA_HISTOGRAM_BOOLEAN("GCM.Crypto.CreateKeySuccessRate", success);
  DCHECK_EQ(0u, key_pairs_.count(app_id));

  if (!success) {
    DVLOG(1) << "Unable to store the created key in the GCM Key Store.";
    callback.Run(KeyPair(), std::string() /* auth_secret */);
    return;
  }

  key_pairs_[app_id] = pair;
  auth_secrets_[app_id] = auth_secret;

  callback.Run(key_pairs_[app_id], auth_secret);
}

void GCMKeyStore::RemoveKeys(const std::string& app_id,
                             const base::Closure& callback) {
  LazyInitialize(base::Bind(&GCMKeyStore::RemoveKeysAfterInitialize,
                            weak_factory_.GetWeakPtr(), app_id, callback));
}

void GCMKeyStore::RemoveKeysAfterInitialize(const std::string& app_id,
                                            const base::Closure& callback) {
  DCHECK(state_ == State::INITIALIZED || state_ == State::FAILED);
  const auto iter = key_pairs_.find(app_id);
  if (iter == key_pairs_.end() || state_ != State::INITIALIZED) {
    callback.Run();
    return;
  }

  using EntryVectorType =
      leveldb_proto::ProtoDatabase<EncryptionData>::KeyEntryVector;

  std::unique_ptr<EntryVectorType> entries_to_save(new EntryVectorType());
  std::unique_ptr<std::vector<std::string>> keys_to_remove(
      new std::vector<std::string>(1, app_id));

  database_->UpdateEntries(
      std::move(entries_to_save), std::move(keys_to_remove),
      base::Bind(&GCMKeyStore::DidRemoveKeys, weak_factory_.GetWeakPtr(),
                 app_id, callback));
}

void GCMKeyStore::DidRemoveKeys(const std::string& app_id,
                                const base::Closure& callback,
                                bool success) {
  UMA_HISTOGRAM_BOOLEAN("GCM.Crypto.RemoveKeySuccessRate", success);

  if (success) {
    key_pairs_.erase(app_id);
    auth_secrets_.erase(app_id);
  } else {
    DVLOG(1) << "Unable to delete a key from the GCM Key Store.";
  }

  callback.Run();
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
  UMA_HISTOGRAM_BOOLEAN("GCM.Crypto.InitKeyStoreSuccessRate", success);
  if (!success) {
    DVLOG(1) << "Unable to initialize the GCM Key Store.";
    state_ = State::FAILED;

    delayed_task_controller_.SetReady();
    return;
  }

  database_->LoadEntries(
      base::Bind(&GCMKeyStore::DidLoadKeys, weak_factory_.GetWeakPtr()));
}

void GCMKeyStore::DidLoadKeys(
    bool success,
    std::unique_ptr<std::vector<EncryptionData>> entries) {
  UMA_HISTOGRAM_BOOLEAN("GCM.Crypto.LoadKeyStoreSuccessRate", success);
  if (!success) {
    DVLOG(1) << "Unable to load entries into the GCM Key Store.";
    state_ = State::FAILED;

    delayed_task_controller_.SetReady();
    return;
  }

  for (const EncryptionData& entry : *entries) {
    DCHECK_EQ(1, entry.keys_size());

    key_pairs_[entry.app_id()] = entry.keys(0);
    auth_secrets_[entry.app_id()] = entry.auth_secret();
  }

  state_ = State::INITIALIZED;

  delayed_task_controller_.SetReady();
}

}  // namespace gcm
