// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_CRYPTO_GCM_KEY_STORE_H_
#define COMPONENTS_GCM_DRIVER_CRYPTO_GCM_KEY_STORE_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/gcm_driver/crypto/proto/gcm_encryption_data.pb.h"
#include "components/gcm_driver/gcm_delayed_task_controller.h"

namespace base {
class SequencedTaskRunner;
}

namespace leveldb_proto {
template <typename T>
class ProtoDatabase;
}

namespace gcm {

// Key storage for use with encrypted messages received from Google Cloud
// Messaging. It provides the ability to create and store a key-pair for a given
// app id + authorized entity pair, and to retrieve and delete key-pairs.
//
// This class is backed by a proto database and might end up doing file I/O on
// a background task runner. For this reason, all public APIs take a callback
// rather than returning the result. Do not rely on the timing of the callbacks.
class GCMKeyStore {
 public:
  using KeysCallback = base::Callback<void(const KeyPair& pair,
                                           const std::string& auth_secret)>;

  GCMKeyStore(
      const base::FilePath& key_store_path,
      const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner);
  ~GCMKeyStore();

  // Retrieves the public/private key-pair associated with the |app_id| +
  // |authorized_entity| pair, and invokes |callback| when they are available,
  // or when an error occurred. |authorized_entity| should be the InstanceID
  // token's authorized entity, or "" for non-InstanceID GCM registrations. If
  // |fallback_to_empty_authorized_entity| is true and the keys are not found,
  // GetKeys will try again with an empty authorized entity; this can be used
  // when it's not known whether or not the |app_id| is for an InstanceID.
  void GetKeys(const std::string& app_id,
               const std::string& authorized_entity,
               bool fallback_to_empty_authorized_entity,
               const KeysCallback& callback);

  // Creates a new public/private key-pair for the |app_id| +
  // |authorized_entity| pair, and invokes |callback| when they are available,
  // or when an error occurred. |authorized_entity| should be the InstanceID
  // token's authorized entity, or "" for non-InstanceID GCM registrations.
  // Simultaneously using the same |app_id| for both a non-InstanceID GCM
  // registration and one or more InstanceID tokens is not supported.
  void CreateKeys(const std::string& app_id,
                  const std::string& authorized_entity,
                  const KeysCallback& callback);

  // Removes the keys associated with the |app_id| + |authorized_entity| pair,
  // and invokes |callback| when the operation has finished. |authorized_entity|
  // should be the InstanceID token's authorized entity, or "*" to remove for
  // all InstanceID tokens, or "" for non-InstanceID GCM registrations.
  void RemoveKeys(const std::string& app_id,
                  const std::string& authorized_entity,
                  const base::Closure& callback);

 private:
  // Initializes the database if necessary, and runs |done_closure| when done.
  void LazyInitialize(const base::Closure& done_closure);

  void DidInitialize(bool success);
  void DidLoadKeys(bool success,
                   std::unique_ptr<std::vector<EncryptionData>> entries);

  void DidStoreKeys(const KeyPair& pair,
                    const std::string& auth_secret,
                    const KeysCallback& callback,
                    bool success);

  void DidRemoveKeys(const base::Closure& callback, bool success);

  // Private implementations of the API that will be executed when the database
  // has either been successfully loaded, or failed to load.

  void GetKeysAfterInitialize(const std::string& app_id,
                              const std::string& authorized_entity,
                              bool fallback_to_empty_authorized_entity,
                              const KeysCallback& callback);
  void CreateKeysAfterInitialize(const std::string& app_id,
                                 const std::string& authorized_entity,
                                 const KeysCallback& callback);
  void RemoveKeysAfterInitialize(const std::string& app_id,
                                 const std::string& authorized_entity,
                                 const base::Closure& callback);

  // Path in which the key store database will be saved.
  base::FilePath key_store_path_;

  // Blocking task runner which the database will do I/O operations on.
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  // Instance of the ProtoDatabase backing the key store.
  std::unique_ptr<leveldb_proto::ProtoDatabase<EncryptionData>> database_;

  enum class State;

  // The current state of the database. It has to be initialized before use.
  State state_;

  // Controller for tasks that should be executed once the key store has
  // finished initializing.
  GCMDelayedTaskController delayed_task_controller_;

  // Nested map from app_id to a map from authorized_entity to the loaded key
  // pair and authentication secrets.
  using KeyPairAndAuthSecret = std::pair<KeyPair, std::string>;
  std::unordered_map<std::string,
                     std::unordered_map<std::string, KeyPairAndAuthSecret>>
      key_data_;

  base::WeakPtrFactory<GCMKeyStore> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GCMKeyStore);
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_CRYPTO_GCM_KEY_STORE_H_
