// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_CRYPTO_GCM_KEY_STORE_H_
#define COMPONENTS_GCM_DRIVER_CRYPTO_GCM_KEY_STORE_H_

#include <map>
#include <memory>
#include <string>
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
// app id, as well as retrieving and deleting key-pairs.
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

  // Retrieves the public/private key-pair associated with |app_id|, and
  // invokes |callback| when they are available, or when an error occurred.
  void GetKeys(const std::string& app_id, const KeysCallback& callback);

  // Creates a new public/private key-pair for |app_id|, and invokes
  // |callback| when they are available, or when an error occurred.
  void CreateKeys(const std::string& app_id, const KeysCallback& callback);

  // Removes the keys associated with |app_id|, and invokes |callback| when
  // the operation has finished.
  void RemoveKeys(const std::string& app_id, const base::Closure& callback);

 private:
  // Initializes the database if necessary, and runs |done_closure| when done.
  void LazyInitialize(const base::Closure& done_closure);

  void DidInitialize(bool success);
  void DidLoadKeys(bool success,
                   std::unique_ptr<std::vector<EncryptionData>> entries);

  void DidStoreKeys(const std::string& app_id,
                    const KeyPair& pair,
                    const std::string& auth_secret,
                    const KeysCallback& callback,
                    bool success);

  void DidRemoveKeys(const std::string& app_id,
                     const base::Closure& callback,
                     bool success);

  // Private implementations of the API that will be executed when the database
  // has either been successfully loaded, or failed to load.

  void GetKeysAfterInitialize(const std::string& app_id,
                              const KeysCallback& callback);
  void CreateKeysAfterInitialize(const std::string& app_id,
                                 const KeysCallback& callback);
  void RemoveKeysAfterInitialize(const std::string& app_id,
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

  // Mapping of an app id to the loaded key pair and authentication secrets.
  // TODO(peter): Switch these to std::unordered_map<> once allowed.
  std::map<std::string, KeyPair> key_pairs_;
  std::map<std::string, std::string> auth_secrets_;

  base::WeakPtrFactory<GCMKeyStore> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GCMKeyStore);
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_CRYPTO_GCM_KEY_STORE_H_
