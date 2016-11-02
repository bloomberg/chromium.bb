// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/crypto/gcm_key_store.h"

#include <stddef.h>

#include <utility>

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "components/gcm_driver/crypto/p256_key_util.h"
#include "components/leveldb_proto/proto_database_impl.h"
#include "crypto/random.h"

namespace gcm {

namespace {

// Statistics are logged to UMA with this string as part of histogram name. They
// can all be found under LevelDB.*.GCMKeyStore. Changing this needs to
// synchronize with histograms.xml, AND will also become incompatible with older
// browsers still reporting the previous values.
const char kDatabaseUMAClientName[] = "GCMKeyStore";

// Number of cryptographically secure random bytes to generate as a key pair's
// authentication secret. Must be at least 16 bytes.
const size_t kAuthSecretBytes = 16;

std::string DatabaseKey(const std::string& app_id,
                        const std::string& authorized_entity) {
  DCHECK_EQ(std::string::npos, app_id.find(','));
  DCHECK_EQ(std::string::npos, authorized_entity.find(','));
  DCHECK_NE("*", authorized_entity) << "Wildcards require special handling";
  return authorized_entity.empty()
             ? app_id  // No comma, for compatibility with existing keys.
             : app_id + ',' + authorized_entity;
}

}  // namespace

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
                          const std::string& authorized_entity,
                          bool fallback_to_empty_authorized_entity,
                          const KeysCallback& callback) {
  LazyInitialize(base::Bind(
      &GCMKeyStore::GetKeysAfterInitialize, weak_factory_.GetWeakPtr(), app_id,
      authorized_entity, fallback_to_empty_authorized_entity, callback));
}

void GCMKeyStore::GetKeysAfterInitialize(
    const std::string& app_id,
    const std::string& authorized_entity,
    bool fallback_to_empty_authorized_entity,
    const KeysCallback& callback) {
  DCHECK(state_ == State::INITIALIZED || state_ == State::FAILED);
  bool success = false;

  if (state_ == State::INITIALIZED) {
    auto outer_iter = key_data_.find(app_id);
    if (outer_iter != key_data_.end()) {
      const auto& inner_map = outer_iter->second;
      auto inner_iter = inner_map.find(authorized_entity);
      if (fallback_to_empty_authorized_entity && inner_iter == inner_map.end())
        inner_iter = inner_map.find(std::string());
      if (inner_iter != inner_map.end()) {
        const KeyPairAndAuthSecret& key_and_auth = inner_iter->second;
        callback.Run(key_and_auth.first, key_and_auth.second);
        success = true;
      }
    }
  }

  UMA_HISTOGRAM_BOOLEAN("GCM.Crypto.GetKeySuccessRate", success);
  if (!success)
    callback.Run(KeyPair(), std::string() /* auth_secret */);
}

void GCMKeyStore::CreateKeys(const std::string& app_id,
                             const std::string& authorized_entity,
                             const KeysCallback& callback) {
  LazyInitialize(base::Bind(&GCMKeyStore::CreateKeysAfterInitialize,
                            weak_factory_.GetWeakPtr(), app_id,
                            authorized_entity, callback));
}

void GCMKeyStore::CreateKeysAfterInitialize(
    const std::string& app_id,
    const std::string& authorized_entity,
    const KeysCallback& callback) {
  DCHECK(state_ == State::INITIALIZED || state_ == State::FAILED);
  if (state_ != State::INITIALIZED) {
    callback.Run(KeyPair(), std::string() /* auth_secret */);
    return;
  }

  // Only allow creating new keys if no keys currently exist. Multiple Instance
  // ID tokens can share an app_id (with different authorized entities), but
  // InstanceID tokens can't share an app_id with a non-InstanceID registration.
  // This invariant is necessary for the fallback_to_empty_authorized_entity
  // mode of GetKey (needed by GCMEncryptionProvider::DecryptMessage, which
  // can't distinguish Instance ID tokens from non-InstanceID registrations).
  DCHECK(!key_data_.count(app_id) ||
         (!authorized_entity.empty() &&
          !key_data_[app_id].count(authorized_entity) &&
          !key_data_[app_id].count(std::string())))
      << "Instance ID tokens cannot share an app_id with a non-InstanceID GCM "
         "registration";

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
  if (!authorized_entity.empty())
    encryption_data.set_authorized_entity(authorized_entity);
  encryption_data.set_auth_secret(auth_secret);

  KeyPair* pair = encryption_data.add_keys();
  pair->set_type(KeyPair::ECDH_P256);
  pair->set_private_key(private_key);
  pair->set_public_key_x509(public_key_x509);
  pair->set_public_key(public_key);

  // Write them immediately to our cache, so subsequent calls to
  // {Get/Create/Remove}Keys can see them.
  key_data_[app_id][authorized_entity] = {*pair, auth_secret};

  using EntryVectorType =
      leveldb_proto::ProtoDatabase<EncryptionData>::KeyEntryVector;

  std::unique_ptr<EntryVectorType> entries_to_save(new EntryVectorType());
  std::unique_ptr<std::vector<std::string>> keys_to_remove(
      new std::vector<std::string>());

  entries_to_save->push_back(
      std::make_pair(DatabaseKey(app_id, authorized_entity), encryption_data));

  database_->UpdateEntries(
      std::move(entries_to_save), std::move(keys_to_remove),
      base::Bind(&GCMKeyStore::DidStoreKeys, weak_factory_.GetWeakPtr(), *pair,
                 auth_secret, callback));
}

void GCMKeyStore::DidStoreKeys(const KeyPair& pair,
                               const std::string& auth_secret,
                               const KeysCallback& callback,
                               bool success) {
  UMA_HISTOGRAM_BOOLEAN("GCM.Crypto.CreateKeySuccessRate", success);

  if (!success) {
    LOG(ERROR) << "Unable to store the created key in the GCM Key Store.";

    // Our cache is now inconsistent. Reject requests until restarted.
    state_ = State::FAILED;

    callback.Run(KeyPair(), std::string() /* auth_secret */);
    return;
  }

  callback.Run(pair, auth_secret);
}

void GCMKeyStore::RemoveKeys(const std::string& app_id,
                             const std::string& authorized_entity,
                             const base::Closure& callback) {
  LazyInitialize(base::Bind(&GCMKeyStore::RemoveKeysAfterInitialize,
                            weak_factory_.GetWeakPtr(), app_id,
                            authorized_entity, callback));
}

void GCMKeyStore::RemoveKeysAfterInitialize(
    const std::string& app_id,
    const std::string& authorized_entity,
    const base::Closure& callback) {
  DCHECK(state_ == State::INITIALIZED || state_ == State::FAILED);

  const auto& outer_iter = key_data_.find(app_id);
  if (outer_iter == key_data_.end() || state_ != State::INITIALIZED) {
    callback.Run();
    return;
  }

  using EntryVectorType =
      leveldb_proto::ProtoDatabase<EncryptionData>::KeyEntryVector;

  std::unique_ptr<EntryVectorType> entries_to_save(new EntryVectorType());
  std::unique_ptr<std::vector<std::string>> keys_to_remove(
      new std::vector<std::string>());

  bool had_keys = false;
  auto& inner_map = outer_iter->second;
  for (auto it = inner_map.begin(); it != inner_map.end();) {
    // Wildcard "*" matches all non-empty authorized entities (InstanceID only).
    if (authorized_entity == "*" ? !it->first.empty()
                                 : it->first == authorized_entity) {
      had_keys = true;

      keys_to_remove->push_back(DatabaseKey(app_id, it->first));

      // Clear keys immediately from our cache, so subsequent calls to
      // {Get/Create/Remove}Keys don't see them.
      it = inner_map.erase(it);
    } else {
      ++it;
    }
  }
  if (!had_keys) {
    callback.Run();
    return;
  }
  if (inner_map.empty())
    key_data_.erase(app_id);

  database_->UpdateEntries(std::move(entries_to_save),
                           std::move(keys_to_remove),
                           base::Bind(&GCMKeyStore::DidRemoveKeys,
                                      weak_factory_.GetWeakPtr(), callback));
}

void GCMKeyStore::DidRemoveKeys(const base::Closure& callback, bool success) {
  UMA_HISTOGRAM_BOOLEAN("GCM.Crypto.RemoveKeySuccessRate", success);

  if (!success) {
    LOG(ERROR) << "Unable to delete a key from the GCM Key Store.";

    // Our cache is now inconsistent. Reject requests until restarted.
    state_ = State::FAILED;
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

    std::string authorized_entity;
    if (entry.has_authorized_entity())
      authorized_entity = entry.authorized_entity();
    key_data_[entry.app_id()][authorized_entity] = {entry.keys(0),
                                                    entry.auth_secret()};
  }

  state_ = State::INITIALIZED;

  delayed_task_controller_.SetReady();
}

}  // namespace gcm
