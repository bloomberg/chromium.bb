// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/crypto/gcm_encryption_provider.h"

#include "base/bind.h"
#include "base/logging.h"
#include "components/gcm_driver/crypto/gcm_key_store.h"
#include "components/gcm_driver/crypto/proto/gcm_encryption_data.pb.h"

namespace gcm {

// Directory in the GCM Store in which the encryption database will be stored.
const base::FilePath::CharType kEncryptionDirectoryName[] =
    FILE_PATH_LITERAL("Encryption");

GCMEncryptionProvider::GCMEncryptionProvider()
    : weak_ptr_factory_(this) {
}

GCMEncryptionProvider::~GCMEncryptionProvider() {
}

void GCMEncryptionProvider::Init(
    const base::FilePath& store_path,
    const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner) {
  DCHECK(!key_store_);

  base::FilePath encryption_store_path = store_path;

  // |store_path| can be empty in tests, which means that the database should
  // be created in memory rather than on-disk.
  if (!store_path.empty())
    encryption_store_path = store_path.Append(kEncryptionDirectoryName);

  key_store_ = new GCMKeyStore(encryption_store_path, blocking_task_runner);
}

void GCMEncryptionProvider::GetPublicKey(const std::string& app_id,
                                         const PublicKeyCallback& callback) {
  DCHECK(key_store_);
  key_store_->GetKeys(
      app_id, base::Bind(&GCMEncryptionProvider::DidGetPublicKey,
                         weak_ptr_factory_.GetWeakPtr(), app_id, callback));
}

void GCMEncryptionProvider::DidGetPublicKey(const std::string& app_id,
                                            const PublicKeyCallback& callback,
                                            const KeyPair& pair) {
  if (!pair.IsInitialized()) {
    key_store_->CreateKeys(
        app_id, base::Bind(&GCMEncryptionProvider::DidCreatePublicKey,
                           weak_ptr_factory_.GetWeakPtr(), callback));
    return;
  }

  DCHECK_EQ(KeyPair::ECDH_CURVE_25519, pair.type());
  callback.Run(pair.public_key());
}

void GCMEncryptionProvider::DidCreatePublicKey(
    const PublicKeyCallback& callback,
    const KeyPair& pair) {
  if (!pair.IsInitialized()) {
    callback.Run(std::string());
    return;
  }

  DCHECK_EQ(KeyPair::ECDH_CURVE_25519, pair.type());
  callback.Run(pair.public_key());
}

}  // namespace gcm
