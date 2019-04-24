// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/nigori/nigori_sync_bridge_impl.h"

#include "base/base64.h"
#include "base/location.h"
#include "components/sync/base/nigori.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/model/entity_data.h"
#include "components/sync/protocol/encryption.pb.h"
#include "components/sync/protocol/nigori_specifics.pb.h"

namespace syncer {

namespace {

// Attempts to decrypt |keystore_decryptor_token| with |keystore_keys|. Returns
// serialized Nigori key if successful and base::nullopt otherwise.
base::Optional<std::string> DecryptKeystoreDecryptor(
    const std::vector<std::string> keystore_keys,
    const sync_pb::EncryptedData& keystore_decryptor_token,
    Encryptor* const encryptor) {
  if (keystore_decryptor_token.blob().empty()) {
    return base::nullopt;
  }

  Cryptographer cryptographer(encryptor);
  for (const std::string& key : keystore_keys) {
    KeyParams key_params = {KeyDerivationParams::CreateForPbkdf2(), key};
    // TODO(crbug.com/922900): possible behavioral change. Old implementation
    // fails only if we failed to add current keystore key. Failing to add any
    // of these keys doesn't seem valid. This line seems to be a good candidate
    // for UMA, as it's not a normal situation, if we fail to add any key.
    if (!cryptographer.AddKey(key_params)) {
      return base::nullopt;
    }
  }

  std::string serialized_nigori_key;
  // This check should never fail as long as we don't receive invalid data.
  if (!cryptographer.CanDecrypt(keystore_decryptor_token) ||
      !cryptographer.DecryptToString(keystore_decryptor_token,
                                     &serialized_nigori_key)) {
    return base::nullopt;
  }
  return serialized_nigori_key;
}

}  // namespace

NigoriSyncBridgeImpl::NigoriSyncBridgeImpl(Encryptor* encryptor)
    : cryptographer_(encryptor) {}

NigoriSyncBridgeImpl::~NigoriSyncBridgeImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NigoriSyncBridgeImpl::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

void NigoriSyncBridgeImpl::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

void NigoriSyncBridgeImpl::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

void NigoriSyncBridgeImpl::SetEncryptionPassphrase(
    const std::string& passphrase) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

void NigoriSyncBridgeImpl::SetDecryptionPassphrase(
    const std::string& passphrase) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

void NigoriSyncBridgeImpl::EnableEncryptEverything() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

bool NigoriSyncBridgeImpl::IsEncryptEverythingEnabled() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  return false;
}

bool NigoriSyncBridgeImpl::NeedKeystoreKey() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // We never need to explicitly ask the server to provide the new keystore
  // key, because server sends keystore keys every time it sends keystore-based
  // Nigori node.
  // TODO(crbug.com/922900): verify logic above. Old implementation is
  // different, but it's probably only related to Nigori migration to keystore
  // logic: there was no need to send new Nigori node from the server, because
  // the migration was implemented on client side, but client needs
  // keystore keys in order to perform the migration.
  return false;
}

bool NigoriSyncBridgeImpl::SetKeystoreKeys(
    const std::vector<std::string>& keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (keys.empty() || keys.back().empty()) {
    return false;
  }

  keystore_keys_.resize(keys.size());
  for (size_t i = 0; i < keys.size(); ++i) {
    // We need to apply base64 encoding before using the keys to provide
    // backward compatibility with non-USS implementation. It's actually needed
    // only for the keys persisting, but was applied before passing keys to
    // cryptographer, so we have to do the same.
    base::Base64Encode(keys[i], &keystore_keys_[i]);
  }

  // TODO(crbug.com/922900): persist keystore keys.
  // TODO(crbug.com/922900): support keystore rotation.
  // TODO(crbug.com/922900): verify that this method is always called before
  // update or init of Nigori node. If this is the case we don't need to touch
  // cryptographer here. If this is not the case, old code is actually broken:
  // 1. Receive and persist the Nigori node after keystore rotation on
  // different client.
  // 2. Browser crash.
  // 3. After load we don't request new Nigori node from the server (we already
  // have the newest one), so logic with simultaneous sending of Nigori node
  // and keystore keys doesn't help. We don't request new keystore keys
  // explicitly (we already have one). We can't decrypt and use Nigori node
  // with old keystore keys.
  NOTIMPLEMENTED();
  return true;
}

base::Optional<ModelError> NigoriSyncBridgeImpl::MergeSyncData(
    const base::Optional<EntityData>& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!data) {
    return ModelError(FROM_HERE,
                      "Received empty EntityData during initial "
                      "sync of Nigori.");
  }
  return ApplySyncChanges(data);
}

base::Optional<ModelError> NigoriSyncBridgeImpl::ApplySyncChanges(
    const base::Optional<EntityData>& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (data) {
    DCHECK(data->specifics.has_nigori());
    const sync_pb::NigoriSpecifics& nigori = data->specifics.nigori();

    // TODO(crbug.com/922900): support other passphrase types.
    if (ProtoPassphraseTypeToEnum(nigori.passphrase_type()) !=
        PassphraseType::KEYSTORE_PASSPHRASE) {
      NOTIMPLEMENTED();
      return ModelError(FROM_HERE, "Only keystore Nigori node is supported.");
    }

    DCHECK(!keystore_keys_.empty());
    // TODO(crbug.com/922900): verify that we don't need to check that
    // nigori.encryption_keybag() and nigori.keystore_decryptor_token() are not
    // empty.
    sync_pb::EncryptedData keybag = nigori.encryption_keybag();
    if (cryptographer_.CanDecrypt(keybag)) {
      cryptographer_.InstallKeys(keybag);
    } else {
      cryptographer_.SetPendingKeys(keybag);
      base::Optional<std::string> serialized_keystore_decryptor =
          DecryptKeystoreDecryptor(keystore_keys_,
                                   nigori.keystore_decryptor_token(),
                                   cryptographer_.encryptor());
      if (!serialized_keystore_decryptor ||
          !cryptographer_.ImportNigoriKey(*serialized_keystore_decryptor) ||
          !cryptographer_.is_ready()) {
        return ModelError(FROM_HERE,
                          "Failed to decrypt pending keys using the keystore "
                          "decryptor token.");
      }
    }
  }
  // TODO(crbug.com/922900): implement updates of other data fields (e.g.
  // passphrase type, encrypted types).
  NOTIMPLEMENTED();
  return base::nullopt;
}

std::unique_ptr<EntityData> NigoriSyncBridgeImpl::GetData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  return nullptr;
}

ConflictResolution NigoriSyncBridgeImpl::ResolveConflict(
    const EntityData& local_data,
    const EntityData& remote_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  return ConflictResolution::kUseLocal;
}

void NigoriSyncBridgeImpl::ApplyDisableSyncChanges() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

const Cryptographer& NigoriSyncBridgeImpl::GetCryptographerForTesting() const {
  return cryptographer_;
}

}  // namespace syncer
