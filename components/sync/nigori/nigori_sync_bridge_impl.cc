// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/nigori/nigori_sync_bridge_impl.h"

#include <utility>

#include "base/base64.h"
#include "base/location.h"
#include "components/sync/base/nigori.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/base/time.h"
#include "components/sync/model/entity_data.h"
#include "components/sync/protocol/encryption.pb.h"
#include "components/sync/protocol/nigori_specifics.pb.h"

namespace syncer {

namespace {

// Attempts to decrypt |keystore_decryptor_token| with |keystore_keys|. Returns
// serialized Nigori key if successful and base::nullopt otherwise.
base::Optional<std::string> DecryptKeystoreDecryptor(
    const std::vector<std::string>& keystore_keys,
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

// Creates keystore Nigori specifics given |keystore_keys|. |encryptor| is used
// only to initialize the cryptographer.
// Returns new NigoriSpecifics if successful and base::nullopt otherwise. If
// successful the result will contain:
// 1. passphrase_type = KEYSTORE_PASSPHRASE.
// 2. encryption_keybag contains all |keystore_keys| and encrypted with the
// latest keystore key.
// 3. keystore_decryptor_token contains latest keystore key encrypted with
// itself.
// 4. keybag_is_frozen = true.
// 5. keystore_migration_time is current time.
// 6. Other fields are default.
base::Optional<sync_pb::NigoriSpecifics> MakeDefaultKeystoreNigori(
    const std::vector<std::string>& keystore_keys,
    Encryptor* encryptor) {
  DCHECK(encryptor);
  DCHECK(!keystore_keys.empty());

  Cryptographer cryptographer(encryptor);
  // The last keystore key will become default.
  for (const std::string& key : keystore_keys) {
    // This check and checks below theoretically should never fail, but in case
    // of failure they should be handled.
    if (!cryptographer.AddKey({KeyDerivationParams::CreateForPbkdf2(), key})) {
      DLOG(ERROR) << "Failed to add keystore key to cryptographer.";
      return base::nullopt;
    }
  }
  sync_pb::NigoriSpecifics specifics;
  if (!cryptographer.EncryptString(
          cryptographer.GetDefaultNigoriKeyData(),
          specifics.mutable_keystore_decryptor_token())) {
    DLOG(ERROR) << "Failed to encrypt default key as keystore_decryptor_token.";
    return base::nullopt;
  }
  if (!cryptographer.GetKeys(specifics.mutable_encryption_keybag())) {
    DLOG(ERROR) << "Failed to encrypt keystore keys into encryption_keybag.";
    return base::nullopt;
  }
  specifics.set_passphrase_type(
      EnumPassphraseTypeToProto(PassphraseType::KEYSTORE_PASSPHRASE));
  // Let non-USS client know, that Nigori doesn't need migration.
  specifics.set_keybag_is_frozen(true);
  specifics.set_keystore_migration_time(TimeToProtoTime(base::Time::Now()));
  return specifics;
}

}  // namespace

NigoriSyncBridgeImpl::NigoriSyncBridgeImpl(
    std::unique_ptr<NigoriLocalChangeProcessor> processor,
    Encryptor* encryptor)
    : processor_(std::move(processor)), cryptographer_(encryptor) {}

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

base::Time NigoriSyncBridgeImpl::GetKeystoreMigrationTime() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  return base::Time();
}

bool NigoriSyncBridgeImpl::NeedKeystoreKey() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // We explicitly ask the server for keystore keys iff it's first-time sync,
  // i.e. if we have no keystore keys yet. In case of key rotation, it's a
  // server responsibility to send updated keystore keys. |keystore_keys_| is
  // expected to be non-empty before MergeSyncData() call, regardless of
  // passphrase type.
  return keystore_keys_.empty();
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
  // TODO(crbug.com/922900): support key rotation.
  // TODO(crbug.com/922900): verify that this method is always called before
  // update or init of Nigori node. If this is the case we don't need to touch
  // cryptographer here. If this is not the case, old code is actually broken:
  // 1. Receive and persist the Nigori node after key rotation on different
  // client.
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
    base::Optional<EntityData> data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!data) {
    return ModelError(FROM_HERE,
                      "Received empty EntityData during initial "
                      "sync of Nigori.");
  }
  DCHECK(data->specifics.has_nigori());
  // Ensure we have |keystore_keys_| during the initial download, requested to
  // the server as per NeedKeystoreKey(), and required for decrypting the
  // keystore_decryptor_token in specifics or initializing the default keystore
  // Nigori.
  if (keystore_keys_.empty()) {
    // TODO(crbug.com/922900): verify, whether it's a valid behavior for custom
    // passphrase.
    return ModelError(FROM_HERE,
                      "Keystore keys are not set during first time sync.");
  }
  if (!data->specifics.nigori().encryption_keybag().blob().empty()) {
    // We received regular Nigori.
    return UpdateLocalState(data->specifics.nigori());
  }
  // We received uninitialized Nigori and need to initialize it as default
  // keystore Nigori.
  base::Optional<sync_pb::NigoriSpecifics> initialized_specifics =
      MakeDefaultKeystoreNigori(keystore_keys_, cryptographer_.encryptor());
  if (!initialized_specifics) {
    return ModelError(FROM_HERE, "Failed to initialize keystore Nigori.");
  }
  *data->specifics.mutable_nigori() = *initialized_specifics;
  processor_->Put(std::make_unique<EntityData>(std::move(*data)));
  return UpdateLocalState(*initialized_specifics);
}

base::Optional<ModelError> NigoriSyncBridgeImpl::ApplySyncChanges(
    base::Optional<EntityData> data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!data) {
    // TODO(crbug.com/922900): persist SyncMetadata and ModelTypeState.
    NOTIMPLEMENTED();
    return base::nullopt;
  }
  DCHECK(data->specifics.has_nigori());
  return UpdateLocalState(data->specifics.nigori());
}

base::Optional<ModelError> NigoriSyncBridgeImpl::UpdateLocalState(
    const sync_pb::NigoriSpecifics& specifics) {
  if (ProtoPassphraseTypeToEnum(specifics.passphrase_type()) !=
      PassphraseType::KEYSTORE_PASSPHRASE) {
    // TODO(crbug.com/922900): support other passphrase types.
    NOTIMPLEMENTED();
    return ModelError(FROM_HERE,
                      "Only keystore Nigori node is currently supported.");
  }
  DCHECK(!keystore_keys_.empty());
  // TODO(crbug.com/922900): sanitize remote update (e.g. ensure, that
  // encryption_keybag() and keystore_decryptor_token() aren't empty).
  const sync_pb::EncryptedData& keybag = specifics.encryption_keybag();
  if (cryptographer_.CanDecrypt(keybag)) {
    // We need to ensure, that |cryptographer_| has all keys.
    cryptographer_.InstallKeys(keybag);
    return base::nullopt;
  }
  // We weren't able to decrypt the keybag with current |cryptographer_| state,
  // but we still can decrypt it with |keystore_keys_|. Note: it's a normal
  // situation, once we perform initial sync or key rotation was performed by
  // another client.
  cryptographer_.SetPendingKeys(keybag);
  base::Optional<std::string> serialized_keystore_decryptor =
      DecryptKeystoreDecryptor(keystore_keys_,
                               specifics.keystore_decryptor_token(),
                               cryptographer_.encryptor());
  if (!serialized_keystore_decryptor ||
      !cryptographer_.ImportNigoriKey(*serialized_keystore_decryptor) ||
      !cryptographer_.is_ready()) {
    return ModelError(FROM_HERE,
                      "Failed to decrypt pending keys using the keystore "
                      "decryptor token.");
  }
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
