// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/nigori/nigori_sync_bridge_impl.h"

#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "components/sync/base/encryptor.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/base/sync_base_switches.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/sync_engine_switches.h"
#include "components/sync/model/entity_data.h"
#include "components/sync/nigori/keystore_keys_cryptographer.h"
#include "components/sync/nigori/nigori.h"
#include "components/sync/nigori/nigori_storage.h"
#include "components/sync/nigori/pending_local_nigori_commit.h"
#include "components/sync/protocol/encryption.pb.h"
#include "components/sync/protocol/nigori_local_data.pb.h"
#include "components/sync/protocol/nigori_specifics.pb.h"

namespace syncer {

namespace {

using sync_pb::NigoriSpecifics;

const char kNigoriNonUniqueName[] = "Nigori";

KeyDerivationMethod GetKeyDerivationMethodFromSpecifics(
    const sync_pb::NigoriSpecifics& specifics) {
  KeyDerivationMethod key_derivation_method = ProtoKeyDerivationMethodToEnum(
      specifics.custom_passphrase_key_derivation_method());
  if (key_derivation_method == KeyDerivationMethod::SCRYPT_8192_8_11 &&
      base::FeatureList::IsEnabled(
          switches::kSyncForceDisableScryptForCustomPassphrase)) {
    // Because scrypt is explicitly disabled, just behave as if it is an
    // unsupported method.
    key_derivation_method = KeyDerivationMethod::UNSUPPORTED;
  }

  return key_derivation_method;
}

std::string GetScryptSaltFromSpecifics(
    const sync_pb::NigoriSpecifics& specifics) {
  DCHECK_EQ(specifics.custom_passphrase_key_derivation_method(),
            sync_pb::NigoriSpecifics::SCRYPT_8192_8_11);
  std::string decoded_salt;
  bool result = base::Base64Decode(
      specifics.custom_passphrase_key_derivation_salt(), &decoded_salt);
  DCHECK(result);
  return decoded_salt;
}

KeyDerivationParams GetKeyDerivationParamsFromSpecifics(
    const sync_pb::NigoriSpecifics& specifics) {
  KeyDerivationMethod method = GetKeyDerivationMethodFromSpecifics(specifics);
  switch (method) {
    case KeyDerivationMethod::PBKDF2_HMAC_SHA1_1003:
      return KeyDerivationParams::CreateForPbkdf2();
    case KeyDerivationMethod::SCRYPT_8192_8_11:
      return KeyDerivationParams::CreateForScrypt(
          GetScryptSaltFromSpecifics(specifics));
    case KeyDerivationMethod::UNSUPPORTED:
      break;
  }

  return KeyDerivationParams::CreateWithUnsupportedMethod();
}

bool SpecificsHasValidKeyDerivationParams(const NigoriSpecifics& specifics) {
  switch (GetKeyDerivationMethodFromSpecifics(specifics)) {
    case KeyDerivationMethod::UNSUPPORTED:
      DLOG(ERROR) << "Unsupported key derivation method encountered: "
                  << specifics.custom_passphrase_key_derivation_method();
      return false;
    case KeyDerivationMethod::PBKDF2_HMAC_SHA1_1003:
      return true;
    case KeyDerivationMethod::SCRYPT_8192_8_11:
      if (!specifics.has_custom_passphrase_key_derivation_salt()) {
        DLOG(ERROR) << "Missed key derivation salt while key derivation "
                    << "method is SCRYPT_8192_8_11.";
        return false;
      }
      std::string temp;
      if (!base::Base64Decode(specifics.custom_passphrase_key_derivation_salt(),
                              &temp)) {
        DLOG(ERROR) << "Key derivation salt is not a valid base64 encoded "
                       "string.";
        return false;
      }
      return true;
  }
}

// Validates given |specifics| assuming it's not specifics received from the
// server during first-time sync for current user (i.e. it's not a default
// specifics).
bool IsValidNigoriSpecifics(const NigoriSpecifics& specifics) {
  if (specifics.encryption_keybag().blob().empty()) {
    DLOG(ERROR) << "Specifics contains empty encryption_keybag.";
    return false;
  }
  switch (ProtoPassphraseInt32ToProtoEnum(specifics.passphrase_type())) {
    case NigoriSpecifics::UNKNOWN:
      DLOG(ERROR) << "Received unknown passphrase type with value: "
                  << specifics.passphrase_type();
      return false;
    case NigoriSpecifics::IMPLICIT_PASSPHRASE:
      return true;
    case NigoriSpecifics::KEYSTORE_PASSPHRASE:
      if (specifics.keystore_decryptor_token().blob().empty()) {
        DLOG(ERROR) << "Keystore Nigori should have filled "
                    << "keystore_decryptor_token.";
        return false;
      }
      break;
    case NigoriSpecifics::CUSTOM_PASSPHRASE:
      if (!SpecificsHasValidKeyDerivationParams(specifics)) {
        return false;
      }
      FALLTHROUGH;
    case NigoriSpecifics::FROZEN_IMPLICIT_PASSPHRASE:
      if (!specifics.encrypt_everything()) {
        DLOG(ERROR) << "Nigori with explicit passphrase type should have "
                       "enabled encrypt_everything.";
        return false;
      }
      break;
    case NigoriSpecifics::TRUSTED_VAULT_PASSPHRASE:
      return base::FeatureList::IsEnabled(
          switches::kSyncSupportTrustedVaultPassphrase);
  }
  return true;
}

bool IsValidPassphraseTransition(
    NigoriSpecifics::PassphraseType old_passphrase_type,
    NigoriSpecifics::PassphraseType new_passphrase_type) {
  // We assume that |new_passphrase_type| is valid.
  DCHECK_NE(new_passphrase_type, NigoriSpecifics::UNKNOWN);

  if (old_passphrase_type == new_passphrase_type) {
    return true;
  }
  switch (old_passphrase_type) {
    case NigoriSpecifics::UNKNOWN:
      // This can happen iff we have not synced local state yet or synced with
      // default NigoriSpecifics, so we accept any valid passphrase type
      // (invalid filtered before).
    case NigoriSpecifics::IMPLICIT_PASSPHRASE:
      return true;
    case NigoriSpecifics::KEYSTORE_PASSPHRASE:
      return new_passphrase_type == NigoriSpecifics::CUSTOM_PASSPHRASE ||
             new_passphrase_type == NigoriSpecifics::TRUSTED_VAULT_PASSPHRASE;
    case NigoriSpecifics::FROZEN_IMPLICIT_PASSPHRASE:
      // There is no client side code which can cause such transition, but
      // technically it's a valid one and can be implemented in the future.
      return new_passphrase_type == NigoriSpecifics::CUSTOM_PASSPHRASE;
    case NigoriSpecifics::CUSTOM_PASSPHRASE:
      return false;
    case NigoriSpecifics::TRUSTED_VAULT_PASSPHRASE:
      // TODO(crbug.com/984940): The below should be allowed for
      // CUSTOM_PASSPHRASE and KEYSTORE_PASSPHRASE but it requires carefully
      // verifying that the client triggering the transition already had access
      // to the trusted vault passphrase (e.g. the new keybag must be a
      // superset of the old and the default key must have changed).
      NOTIMPLEMENTED();
      return false;
  }
  NOTREACHED();
  return false;
}

// Updates |*current_type| if needed. Returns true if its value was changed.
bool UpdatePassphraseType(NigoriSpecifics::PassphraseType new_type,
                          NigoriSpecifics::PassphraseType* current_type) {
  DCHECK(current_type);
  DCHECK(IsValidPassphraseTransition(*current_type, new_type));
  if (*current_type == new_type) {
    return false;
  }
  *current_type = new_type;
  return true;
}

bool IsValidEncryptedTypesTransition(bool old_encrypt_everything,
                                     const NigoriSpecifics& specifics) {
  // We don't support relaxing the encryption requirements.
  // TODO(crbug.com/922900): more logic is to be added here, once we support
  // enforced encryption for individual datatypes.
  return specifics.encrypt_everything() || !old_encrypt_everything;
}

// Updates |*current_encrypt_everything| if needed. Returns true if its value
// was changed.
bool UpdateEncryptedTypes(const NigoriSpecifics& specifics,
                          bool* current_encrypt_everything) {
  DCHECK(current_encrypt_everything);
  DCHECK(
      IsValidEncryptedTypesTransition(*current_encrypt_everything, specifics));
  // TODO(crbug.com/922900): more logic is to be added here, once we support
  // enforced encryption for individual datatypes.
  if (*current_encrypt_everything == specifics.encrypt_everything()) {
    return false;
  }
  *current_encrypt_everything = specifics.encrypt_everything();
  return true;
}

// Packs explicit passphrase key in order to persist it. Should be aligned with
// Directory implementation (Cryptographer::GetBootstrapToken()) unless it is
// removed. Returns empty string in case of errors.
std::string PackExplicitPassphraseKey(const Encryptor& encryptor,
                                      const CryptographerImpl& cryptographer) {
  DCHECK(cryptographer.CanEncrypt());

  // Explicit passphrase key should always be default one.
  std::string serialized_key =
      cryptographer.ExportDefaultKey().SerializeAsString();

  if (serialized_key.empty()) {
    DLOG(ERROR) << "Failed to serialize explicit passphrase key.";
    return std::string();
  }

  std::string encrypted_key;
  if (!encryptor.EncryptString(serialized_key, &encrypted_key)) {
    DLOG(ERROR) << "Failed to encrypt explicit passphrase key.";
    return std::string();
  }

  std::string encoded_key;
  base::Base64Encode(encrypted_key, &encoded_key);
  return encoded_key;
}

// Unpacks explicit passphrase keys. Returns a populated sync_pb::NigoriKey if
// successful, or an empty instance (i.e. default value) if |packed_key| is
// empty or decoding/decryption errors occur.
// Should be aligned with Directory implementation (
// Cryptographer::UnpackBootstrapToken()) unless it is removed.
sync_pb::NigoriKey UnpackExplicitPassphraseKey(const Encryptor& encryptor,
                                               const std::string& packed_key) {
  if (packed_key.empty()) {
    return sync_pb::NigoriKey();
  }

  std::string decoded_key;
  if (!base::Base64Decode(packed_key, &decoded_key)) {
    DLOG(ERROR) << "Failed to decode explicit passphrase key.";
    return sync_pb::NigoriKey();
  }

  std::string decrypted_key;
  if (!encryptor.DecryptString(decoded_key, &decrypted_key)) {
    DLOG(ERROR) << "Failed to decrypt expliciti passphrase key.";
    return sync_pb::NigoriKey();
  }

  sync_pb::NigoriKey key;
  key.ParseFromString(decrypted_key);
  return key;
}

ModelTypeSet GetEncryptedTypes(bool encrypt_everything) {
  if (encrypt_everything) {
    return EncryptableUserTypes();
  }
  return SyncEncryptionHandler::SensitiveTypes();
}

}  // namespace

class NigoriSyncBridgeImpl::BroadcastingObserver
    : public SyncEncryptionHandler::Observer {
 public:
  explicit BroadcastingObserver(
      const base::RepeatingClosure& post_passphrase_accepted_cb)
      : post_passphrase_accepted_cb_(post_passphrase_accepted_cb) {}

  ~BroadcastingObserver() override = default;

  void AddObserver(SyncEncryptionHandler::Observer* observer) {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(SyncEncryptionHandler::Observer* observer) {
    observers_.RemoveObserver(observer);
  }

  // SyncEncryptionHandler::Observer implementation.
  void OnPassphraseRequired(
      PassphraseRequiredReason reason,
      const KeyDerivationParams& key_derivation_params,
      const sync_pb::EncryptedData& pending_keys) override {
    for (auto& observer : observers_) {
      observer.OnPassphraseRequired(reason, key_derivation_params,
                                    pending_keys);
    }
  }

  void OnPassphraseAccepted() override {
    for (auto& observer : observers_) {
      observer.OnPassphraseAccepted();
    }
    post_passphrase_accepted_cb_.Run();
  }

  void OnTrustedVaultKeyRequired() override {
    for (auto& observer : observers_) {
      observer.OnTrustedVaultKeyRequired();
    }
  }

  void OnTrustedVaultKeyAccepted() override {
    for (auto& observer : observers_) {
      observer.OnTrustedVaultKeyAccepted();
    }
  }

  void OnBootstrapTokenUpdated(const std::string& bootstrap_token,
                               BootstrapTokenType type) override {
    for (auto& observer : observers_) {
      observer.OnBootstrapTokenUpdated(bootstrap_token, type);
    }
  }

  void OnEncryptedTypesChanged(ModelTypeSet encrypted_types,
                               bool encrypt_everything) override {
    for (auto& observer : observers_) {
      observer.OnEncryptedTypesChanged(encrypted_types, encrypt_everything);
    }
  }

  void OnEncryptionComplete() override {
    for (auto& observer : observers_) {
      observer.OnEncryptionComplete();
    }
  }

  void OnCryptographerStateChanged(Cryptographer* cryptographer,
                                   bool has_pending_keys) override {
    for (auto& observer : observers_) {
      observer.OnCryptographerStateChanged(cryptographer, has_pending_keys);
    }
  }

  void OnPassphraseTypeChanged(PassphraseType type,
                               base::Time passphrase_time) override {
    for (auto& observer : observers_) {
      observer.OnPassphraseTypeChanged(type, passphrase_time);
    }
  }

 private:
  // TODO(crbug/922900): consider using checked ObserverList once
  // SyncEncryptionHandlerImpl is no longer needed or consider refactoring old
  // implementation to use checked ObserverList as well.
  base::ObserverList<SyncEncryptionHandler::Observer>::Unchecked observers_;

  const base::RepeatingClosure post_passphrase_accepted_cb_;

  DISALLOW_COPY_AND_ASSIGN(BroadcastingObserver);
};

NigoriSyncBridgeImpl::NigoriSyncBridgeImpl(
    std::unique_ptr<NigoriLocalChangeProcessor> processor,
    std::unique_ptr<NigoriStorage> storage,
    const Encryptor* encryptor,
    const base::RepeatingCallback<std::string()>& random_salt_generator,
    const std::string& packed_explicit_passphrase_key)
    : encryptor_(encryptor),
      processor_(std::move(processor)),
      storage_(std::move(storage)),
      random_salt_generator_(random_salt_generator),
      explicit_passphrase_key_(
          UnpackExplicitPassphraseKey(*encryptor,
                                      packed_explicit_passphrase_key)),
      broadcasting_observer_(std::make_unique<BroadcastingObserver>(
          // base::Unretained() legit because the observer gets destroyed
          // together with |this|.
          base::BindRepeating(
              &NigoriSyncBridgeImpl::MaybeNotifyBootstrapTokenUpdated,
              base::Unretained(this)))) {
  DCHECK(encryptor);

  // TODO(crbug.com/922900): we currently don't verify |deserialized_data|.
  // It's quite unlikely we get a corrupted data, since it was successfully
  // deserialized and decrypted. But we may want to consider some
  // verifications, taking into account sensitivity of this data.
  base::Optional<sync_pb::NigoriLocalData> deserialized_data =
      storage_->RestoreData();
  if (!deserialized_data ||
      (deserialized_data->nigori_model().passphrase_type() ==
           NigoriSpecifics::TRUSTED_VAULT_PASSPHRASE &&
       !base::FeatureList::IsEnabled(
           switches::kSyncSupportTrustedVaultPassphrase))) {
    // We either have no Nigori node stored locally or it was corrupted.
    processor_->ModelReadyToSync(this, NigoriMetadataBatch());
    return;
  }

  // Restore data.
  state_ = syncer::NigoriState::CreateFromLocalProto(
      deserialized_data->nigori_model());

  // Restore metadata.
  NigoriMetadataBatch metadata_batch;
  metadata_batch.model_type_state = deserialized_data->model_type_state();
  metadata_batch.entity_metadata = deserialized_data->entity_metadata();
  processor_->ModelReadyToSync(this, std::move(metadata_batch));
}

NigoriSyncBridgeImpl::~NigoriSyncBridgeImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NigoriSyncBridgeImpl::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  broadcasting_observer_->AddObserver(observer);
}

void NigoriSyncBridgeImpl::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  broadcasting_observer_->RemoveObserver(observer);
}

bool NigoriSyncBridgeImpl::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // We need to expose whole bridge state through notifications, because it
  // can be different from default due to restoring from the file or
  // completeness of first sync cycle (which happens before Init() call).
  // TODO(crbug.com/922900): try to avoid double notification (second one can
  // happen during UpdateLocalState() call).
  broadcasting_observer_->OnEncryptedTypesChanged(
      GetEncryptedTypes(state_.encrypt_everything), state_.encrypt_everything);
  broadcasting_observer_->OnCryptographerStateChanged(
      state_.cryptographer.get(), state_.pending_keys.has_value());

  MaybeNotifyOfPendingKeys();

  if (state_.passphrase_type != NigoriSpecifics::UNKNOWN) {
    // if |passphrase_type| is unknown, it is not yet initialized and we
    // shouldn't expose it.
    PassphraseType enum_passphrase_type =
        *ProtoPassphraseInt32ToEnum(state_.passphrase_type);
    broadcasting_observer_->OnPassphraseTypeChanged(
        enum_passphrase_type, GetExplicitPassphraseTime());
    UMA_HISTOGRAM_ENUMERATION("Sync.PassphraseType", enum_passphrase_type);
  }
  UMA_HISTOGRAM_BOOLEAN("Sync.CryptographerReady",
                        state_.cryptographer->CanEncrypt());
  UMA_HISTOGRAM_BOOLEAN("Sync.CryptographerPendingKeys",
                        state_.pending_keys.has_value());
  if (state_.pending_keys.has_value() &&
      state_.passphrase_type == NigoriSpecifics::KEYSTORE_PASSPHRASE) {
    // If this is happening, it means the keystore decryptor is either
    // undecryptable with the available keystore keys or does not match the
    // nigori keybag's encryption key. Otherwise we're simply missing the
    // keystore key.
    UMA_HISTOGRAM_BOOLEAN("Sync.KeystoreDecryptionFailed",
                          !state_.keystore_keys_cryptographer->IsEmpty());
  }
  return true;
}

void NigoriSyncBridgeImpl::SetEncryptionPassphrase(
    const std::string& passphrase) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  QueuePendingLocalCommit(PendingLocalNigoriCommit::ForSetCustomPassphrase(
      passphrase, random_salt_generator_));
}

void NigoriSyncBridgeImpl::SetDecryptionPassphrase(
    const std::string& passphrase) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // |passphrase| should be a valid one already (verified by SyncServiceCrypto,
  // using pending keys exposed by OnPassphraseRequired()).
  DCHECK(!passphrase.empty());
  if (!state_.pending_keys) {
    DCHECK_EQ(state_.passphrase_type, NigoriSpecifics::KEYSTORE_PASSPHRASE);
    return;
  }

  const std::string new_key_name = state_.cryptographer->EmplaceKey(
      passphrase, GetKeyDerivationParamsForPendingKeys());
  if (new_key_name.empty()) {
    processor_->ReportError(ModelError(
        FROM_HERE, "Failed to add decryption passphrase to cryptographer."));
    return;
  }

  if (!TryDecryptPendingKeys()) {
    // |pending_keys| could be changed in between of OnPassphraseRequired()
    // and SetDecryptionPassphrase() calls (remote update with different
    // keystore Nigori or with transition from keystore to custom passphrase
    // Nigori).
    MaybeNotifyOfPendingKeys();
    return;
  }

  state_.cryptographer->SelectDefaultEncryptionKey(new_key_name);

  storage_->StoreData(SerializeAsNigoriLocalData());
  broadcasting_observer_->OnCryptographerStateChanged(
      state_.cryptographer.get(), state_.pending_keys.has_value());
  broadcasting_observer_->OnPassphraseAccepted();

  // TODO(crbug.com/922900): we may need to rewrite encryption_keybag in Nigori
  // node in case we have some keys in |cryptographer_| which is not stored in
  // encryption_keybag yet.
  NOTIMPLEMENTED();
}

void NigoriSyncBridgeImpl::AddTrustedVaultDecryptionKeys(
    const std::vector<std::string>& keys) {
  // This API gets plumbed and ultimately exposed to layers outside the sync
  // codebase and even outside the browser, so there are no preconditions and
  // instead we ignore invalid or partially invalid input.
  if (state_.passphrase_type != NigoriSpecifics::TRUSTED_VAULT_PASSPHRASE ||
      !state_.pending_keys || keys.empty()) {
    return;
  }

  for (const std::string& key : keys) {
    if (!key.empty()) {
      state_.cryptographer->EmplaceKey(key,
                                       GetKeyDerivationParamsForPendingKeys());
    }
  }

  const std::string pending_key_name = state_.pending_keys->key_name();

  if (TryDecryptPendingKeys()) {
    state_.cryptographer->SelectDefaultEncryptionKey(pending_key_name);
  }

  storage_->StoreData(SerializeAsNigoriLocalData());

  broadcasting_observer_->OnCryptographerStateChanged(
      state_.cryptographer.get(), state_.pending_keys.has_value());

  if (!state_.pending_keys) {
    broadcasting_observer_->OnTrustedVaultKeyAccepted();
  }

  MaybeNotifyBootstrapTokenUpdated();
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
  return state_.keystore_migration_time;
}

KeystoreKeysHandler* NigoriSyncBridgeImpl::GetKeystoreKeysHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return this;
}

std::string NigoriSyncBridgeImpl::GetLastKeystoreKey() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const std::vector<std::string> keystore_keys =
      state_.keystore_keys_cryptographer->keystore_keys();
  if (keystore_keys.empty()) {
    return std::string();
  }
  return keystore_keys.back();
}

bool NigoriSyncBridgeImpl::NeedKeystoreKey() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // We explicitly ask the server for keystore keys iff it's first-time sync,
  // i.e. if we have no keystore keys yet. In case of key rotation, it's a
  // server responsibility to send updated keystore keys. |keystore_keys_| is
  // expected to be non-empty before MergeSyncData() call, regardless of
  // passphrase type.
  return state_.keystore_keys_cryptographer->IsEmpty() ||
         state_.pending_keystore_decryptor_token.has_value();
}

bool NigoriSyncBridgeImpl::SetKeystoreKeys(
    const std::vector<std::string>& keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (keys.empty() || keys.back().empty()) {
    return false;
  }

  std::vector<std::string> encoded_keystore_keys(keys.size());
  for (size_t i = 0; i < keys.size(); ++i) {
    // We need to apply base64 encoding before using the keys to provide
    // backward compatibility with non-USS implementation. It's actually needed
    // only for the keys persisting, but was applied before passing keys to
    // cryptographer, so we have to do the same.
    base::Base64Encode(keys[i], &encoded_keystore_keys[i]);
  }

  state_.keystore_keys_cryptographer =
      KeystoreKeysCryptographer::FromKeystoreKeys(encoded_keystore_keys);
  if (!state_.keystore_keys_cryptographer) {
    state_.keystore_keys_cryptographer =
        KeystoreKeysCryptographer::CreateEmpty();
    return false;
  }

  if (state_.pending_keystore_decryptor_token.has_value()) {
    // Newly arrived keystore keys could resolve pending encryption state in
    // keystore mode.
    DCHECK_EQ(state_.passphrase_type, NigoriSpecifics::KEYSTORE_PASSPHRASE);
    UpdateCryptographerFromKeystoreNigori(
        sync_pb::EncryptedData(*state_.pending_keys),
        sync_pb::EncryptedData(*state_.pending_keystore_decryptor_token));
    broadcasting_observer_->OnCryptographerStateChanged(
        state_.cryptographer.get(), state_.pending_keys.has_value());
  }
  // Note: we don't need to persist keystore keys here, because we will receive
  // Nigori node right after this method and persist all the data during
  // UpdateLocalState().
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

  if (!data->specifics.nigori().encryption_keybag().blob().empty()) {
    // We received regular Nigori.
    return UpdateLocalState(data->specifics.nigori());
  }
  // Ensure we have |keystore_keys| during the initial download, requested to
  // the server as per NeedKeystoreKey(), and required for initializing the
  // default keystore Nigori.
  DCHECK(state_.keystore_keys_cryptographer);
  if (state_.keystore_keys_cryptographer->IsEmpty()) {
    // TODO(crbug.com/922900): try to relax this requirement for Nigori
    // initialization as well. Keystore keys might not arrive, for example, due
    // to throttling.
    return ModelError(FROM_HERE,
                      "Keystore keys are not set during first time sync.");
  }
  // We received uninitialized Nigori and need to initialize it as default
  // keystore Nigori.
  QueuePendingLocalCommit(
      PendingLocalNigoriCommit::ForKeystoreInitialization());
  return base::nullopt;
}

base::Optional<ModelError> NigoriSyncBridgeImpl::ApplySyncChanges(
    base::Optional<EntityData> data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (data) {
    DCHECK(data->specifics.has_nigori());
    return UpdateLocalState(data->specifics.nigori());
  }

  if (!pending_local_commit_queue_.empty() && !processor_->IsEntityUnsynced()) {
    // Successfully committed first element in queue.
    bool success = pending_local_commit_queue_.front()->TryApply(&state_);
    DCHECK(success);
    pending_local_commit_queue_.front()->OnSuccess(
        state_, broadcasting_observer_.get());
    pending_local_commit_queue_.pop_front();

    // Advance until the next applicable local change if any and call Put().
    PutNextApplicablePendingLocalCommit();
  }

  // Receiving empty |data| means metadata-only change (e.g. no remote updates,
  // or local commit completion), so we need to persist its state.
  storage_->StoreData(SerializeAsNigoriLocalData());
  return base::nullopt;
}

base::Optional<ModelError> NigoriSyncBridgeImpl::UpdateLocalState(
    const NigoriSpecifics& specifics) {
  if (!IsValidNigoriSpecifics(specifics)) {
    return ModelError(FROM_HERE, "NigoriSpecifics is not valid.");
  }

  const sync_pb::NigoriSpecifics::PassphraseType new_passphrase_type =
      ProtoPassphraseInt32ToProtoEnum(specifics.passphrase_type());
  DCHECK_NE(new_passphrase_type, NigoriSpecifics::UNKNOWN);

  if (!IsValidPassphraseTransition(
          /*old_passphrase_type=*/state_.passphrase_type,
          new_passphrase_type)) {
    return ModelError(FROM_HERE, "Invalid passphrase type transition.");
  }
  if (!IsValidEncryptedTypesTransition(state_.encrypt_everything, specifics)) {
    return ModelError(FROM_HERE, "Invalid encrypted types transition.");
  }

  const bool passphrase_type_changed =
      UpdatePassphraseType(new_passphrase_type, &state_.passphrase_type);
  if (passphrase_type_changed) {
    // Keystore decryptor token is relevant only for keystore nigori, so it's
    // safe to clear its pending state now.
    state_.pending_keystore_decryptor_token.reset();
  }
  DCHECK_NE(state_.passphrase_type, NigoriSpecifics::UNKNOWN);

  const bool encrypted_types_changed =
      UpdateEncryptedTypes(specifics, &state_.encrypt_everything);

  if (specifics.has_custom_passphrase_time()) {
    state_.custom_passphrase_time =
        ProtoTimeToTime(specifics.custom_passphrase_time());
  }
  if (specifics.has_keystore_migration_time()) {
    state_.keystore_migration_time =
        ProtoTimeToTime(specifics.keystore_migration_time());
  }

  const sync_pb::EncryptedData& encryption_keybag =
      specifics.encryption_keybag();
  switch (state_.passphrase_type) {
    case NigoriSpecifics::UNKNOWN:
      // NigoriSpecifics with UNKNOWN type is not valid and shouldn't reach
      // this codepath. We just updated |passphrase_type_| from specifics, so
      // it can't be in this state as well.
      NOTREACHED();
      break;
    case NigoriSpecifics::KEYSTORE_PASSPHRASE: {
      base::Optional<ModelError> error = UpdateCryptographerFromKeystoreNigori(
          encryption_keybag, specifics.keystore_decryptor_token());
      if (error) {
        return error;
      }
      break;
    }
    case NigoriSpecifics::CUSTOM_PASSPHRASE:
      state_.custom_passphrase_key_derivation_params =
          GetKeyDerivationParamsFromSpecifics(specifics);
      FALLTHROUGH;
    case NigoriSpecifics::IMPLICIT_PASSPHRASE:
    case NigoriSpecifics::FROZEN_IMPLICIT_PASSPHRASE:
    case NigoriSpecifics::TRUSTED_VAULT_PASSPHRASE:
      UpdateCryptographerFromNonKeystoreNigori(encryption_keybag);
  }

  if (passphrase_type_changed) {
    broadcasting_observer_->OnPassphraseTypeChanged(
        *ProtoPassphraseInt32ToEnum(state_.passphrase_type),
        GetExplicitPassphraseTime());
  }
  if (encrypted_types_changed) {
    // Currently the only way to change encrypted types is to enable
    // encrypt_everything.
    DCHECK(state_.encrypt_everything);
    broadcasting_observer_->OnEncryptedTypesChanged(EncryptableUserTypes(),
                                                    state_.encrypt_everything);
  }
  broadcasting_observer_->OnCryptographerStateChanged(
      state_.cryptographer.get(), state_.pending_keys.has_value());

  MaybeNotifyOfPendingKeys();

  // TODO(crbug.com/): Instead of issuing errors for local commits, call
  // PutNextApplicablePendingLocalCommit() to verify pending local changes
  // (relevant for the conflict case). Issue failures if they are no longer
  // applicable, or call Put() otherwise.
  for (const auto& pending_local_commit : pending_local_commit_queue_) {
    pending_local_commit->OnFailure(broadcasting_observer_.get());
  }
  pending_local_commit_queue_.clear();

  storage_->StoreData(SerializeAsNigoriLocalData());

  return base::nullopt;
}

base::Optional<ModelError>
NigoriSyncBridgeImpl::UpdateCryptographerFromKeystoreNigori(
    const sync_pb::EncryptedData& encryption_keybag,
    const sync_pb::EncryptedData& keystore_decryptor_token) {
  DCHECK(!encryption_keybag.blob().empty());
  DCHECK(!keystore_decryptor_token.blob().empty());

  // Decryption of |keystore_decryptor_token|.
  NigoriKeyBag keystore_decryptor_key_bag = NigoriKeyBag::CreateEmpty();
  sync_pb::NigoriKey keystore_decryptor_key;
  if (state_.keystore_keys_cryptographer->DecryptKeystoreDecryptorToken(
          keystore_decryptor_token, &keystore_decryptor_key)) {
    keystore_decryptor_key_bag.AddKeyFromProto(keystore_decryptor_key);
    state_.pending_keystore_decryptor_token.reset();
  } else {
    state_.pending_keystore_decryptor_token = keystore_decryptor_token;
  }

  // TODO(crbug.com/922900): issue ModelError if |keystore_decryptor_keybag| is
  // not empty and can't decrypt the |encryption_keybag|?
  if (keystore_decryptor_key_bag.CanDecrypt(encryption_keybag)) {
    // |encryption_keybag| must contain the key it was encrypted with, so it's
    // okay to add it earlier.
    state_.cryptographer->EmplaceKeysFrom(keystore_decryptor_key_bag);
  }

  // Decryption of |encryption_keybag|.
  sync_pb::NigoriKeyBag key_bag;
  if (!state_.cryptographer->Decrypt(encryption_keybag, &key_bag)) {
    state_.cryptographer->ClearDefaultEncryptionKey();
    state_.pending_keys = encryption_keybag;
    return base::nullopt;
  }

  state_.cryptographer->EmplaceKeysFrom(NigoriKeyBag::CreateFromProto(key_bag));
  state_.cryptographer->SelectDefaultEncryptionKey(
      encryption_keybag.key_name());
  if (state_.pending_keys) {
    state_.pending_keys.reset();
    broadcasting_observer_->OnPassphraseAccepted();
  }
  return base::nullopt;
}

void NigoriSyncBridgeImpl::UpdateCryptographerFromNonKeystoreNigori(
    const sync_pb::EncryptedData& encryption_keybag) {
  // TODO(crbug.com/922900): support the case when client knows passphrase.
  NOTIMPLEMENTED();
  DCHECK(!encryption_keybag.blob().empty());

  // TODO(crbug.com/922900): consider detection of protocol violation instead
  // of cleaning previously set |pending_keys|. If they was set, they should
  // remain set after exiting this function (but might have different value).
  // Clean up previous pending keys state.
  state_.pending_keys.reset();

  if (!state_.cryptographer->CanDecrypt(encryption_keybag)) {
    // Historically, prior to USS, key derived from explicit passphrase was
    // stored in prefs and effectively we do migration here.
    NigoriKeyBag key_bag = NigoriKeyBag::CreateEmpty();
    const std::string key_name =
        key_bag.AddKeyFromProto(explicit_passphrase_key_);
    if (key_bag.CanDecrypt(encryption_keybag)) {
      state_.cryptographer->EmplaceKeysFrom(key_bag);
      DCHECK(state_.cryptographer->CanDecrypt(encryption_keybag));
      state_.cryptographer->SelectDefaultEncryptionKey(key_name);
    } else {
      // This will lead to OnPassphraseRequired() call later.
      state_.pending_keys = encryption_keybag;
      state_.cryptographer->ClearDefaultEncryptionKey();
      return;
    }
  }
  // |cryptographer_| can already have explicit passphrase, in that case it
  // should be able to decrypt |encryption_keybag|. We need to take keys from
  // |encryption_keybag| since some other client can write old keys to
  // |encryption_keybag| and could encrypt some data with them.
  // TODO(crbug.com/922900): find and document at least one real case
  // corresponding to the sentence above.
  // TODO(crbug.com/922900): we may also need to rewrite Nigori with keys
  // currently stored in cryptographer, in case it doesn't have them already.
  sync_pb::NigoriKeyBag key_bag;
  state_.cryptographer->Decrypt(encryption_keybag, &key_bag);
  state_.cryptographer->EmplaceKeysFrom(NigoriKeyBag::CreateFromProto(key_bag));
}

bool NigoriSyncBridgeImpl::TryDecryptPendingKeys() {
  sync_pb::NigoriKeyBag decrypted_pending_keys;
  if (!state_.cryptographer->Decrypt(*state_.pending_keys,
                                     &decrypted_pending_keys)) {
    return false;
  }
  state_.cryptographer->EmplaceKeysFrom(
      NigoriKeyBag::CreateFromProto(decrypted_pending_keys));
  state_.pending_keys.reset();
  return true;
}

std::unique_ptr<EntityData> NigoriSyncBridgeImpl::GetData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  NigoriSpecifics specifics;
  if (!pending_local_commit_queue_.empty()) {
    NigoriState changed_state = state_.Clone();
    bool success =
        pending_local_commit_queue_.front()->TryApply(&changed_state);
    DCHECK(success);
    specifics = changed_state.ToSpecificsProto();
  } else {
    specifics = state_.ToSpecificsProto();
  }

  if (specifics.passphrase_type() == NigoriSpecifics::UNKNOWN) {
    // Bridge never received NigoriSpecifics from the server. This line should
    // be reachable only from processor's GetAllNodesForDebugging().
    DCHECK(!state_.cryptographer->CanEncrypt());
    DCHECK(!state_.pending_keys.has_value());
    return nullptr;
  }

  DCHECK(IsValidNigoriSpecifics(specifics));

  auto entity_data = std::make_unique<EntityData>();
  *entity_data->specifics.mutable_nigori() = std::move(specifics);
  entity_data->name = kNigoriNonUniqueName;
  entity_data->is_folder = true;
  return entity_data;
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
  // The user intended to disable sync, so we need to clear all the data, except
  // |explicit_passphrase_key_| in memory, because this function can be called
  // due to BackendMigrator. It's safe even if this function called due to
  // actual disabling sync, since we remove the persisted key by clearing sync
  // prefs explicitly, and don't reuse current instance of the bridge after
  // that.
  // TODO(crbug.com/922900): idea with keeping
  // |explicit_passphrase_key_| will become not working, once we clean up
  // storing explicit passphrase key in prefs, we need to find better solution.
  storage_->ClearData();
  state_.keystore_keys_cryptographer = KeystoreKeysCryptographer::CreateEmpty();
  state_.cryptographer = CryptographerImpl::CreateEmpty();
  state_.pending_keys.reset();
  state_.pending_keystore_decryptor_token.reset();
  state_.passphrase_type = NigoriSpecifics::UNKNOWN;
  state_.encrypt_everything = false;
  state_.custom_passphrase_time = base::Time();
  state_.keystore_migration_time = base::Time();
  state_.custom_passphrase_key_derivation_params = base::nullopt;
  broadcasting_observer_->OnCryptographerStateChanged(
      state_.cryptographer.get(),
      /*has_pending_keys=*/false);
  broadcasting_observer_->OnEncryptedTypesChanged(SensitiveTypes(), false);
}

const Cryptographer& NigoriSyncBridgeImpl::GetCryptographerForTesting() const {
  return *state_.cryptographer;
}

sync_pb::NigoriSpecifics::PassphraseType
NigoriSyncBridgeImpl::GetPassphraseTypeForTesting() const {
  return state_.passphrase_type;
}

ModelTypeSet NigoriSyncBridgeImpl::GetEncryptedTypesForTesting() const {
  return GetEncryptedTypes(state_.encrypt_everything);
}

bool NigoriSyncBridgeImpl::HasPendingKeysForTesting() const {
  return state_.pending_keys.has_value();
}

std::string NigoriSyncBridgeImpl::PackExplicitPassphraseKeyForTesting(
    const Encryptor& encryptor,
    const CryptographerImpl& cryptographer) {
  return PackExplicitPassphraseKey(encryptor, cryptographer);
}

base::Time NigoriSyncBridgeImpl::GetExplicitPassphraseTime() const {
  switch (state_.passphrase_type) {
    case NigoriSpecifics::IMPLICIT_PASSPHRASE:
    case NigoriSpecifics::UNKNOWN:
    case NigoriSpecifics::KEYSTORE_PASSPHRASE:
    case NigoriSpecifics::TRUSTED_VAULT_PASSPHRASE:
      return base::Time();
    case NigoriSpecifics::FROZEN_IMPLICIT_PASSPHRASE:
      return state_.keystore_migration_time;
    case NigoriSpecifics::CUSTOM_PASSPHRASE:
      return state_.custom_passphrase_time;
  }
  NOTREACHED();
  return state_.custom_passphrase_time;
}

KeyDerivationParams NigoriSyncBridgeImpl::GetKeyDerivationParamsForPendingKeys()
    const {
  switch (state_.passphrase_type) {
    case NigoriSpecifics::UNKNOWN:
      NOTREACHED();
      return KeyDerivationParams::CreateWithUnsupportedMethod();
    case NigoriSpecifics::IMPLICIT_PASSPHRASE:
    case NigoriSpecifics::KEYSTORE_PASSPHRASE:
    case NigoriSpecifics::FROZEN_IMPLICIT_PASSPHRASE:
    case NigoriSpecifics::TRUSTED_VAULT_PASSPHRASE:
      return KeyDerivationParams::CreateForPbkdf2();
    case NigoriSpecifics::CUSTOM_PASSPHRASE:
      DCHECK(state_.custom_passphrase_key_derivation_params);
      return *state_.custom_passphrase_key_derivation_params;
  }
}

void NigoriSyncBridgeImpl::MaybeNotifyOfPendingKeys() const {
  if (!state_.pending_keys.has_value()) {
    return;
  }

  switch (state_.passphrase_type) {
    case NigoriSpecifics::UNKNOWN:
      return;
    case NigoriSpecifics::IMPLICIT_PASSPHRASE:
    case NigoriSpecifics::KEYSTORE_PASSPHRASE:
    case NigoriSpecifics::CUSTOM_PASSPHRASE:
    case NigoriSpecifics::FROZEN_IMPLICIT_PASSPHRASE:
      broadcasting_observer_->OnPassphraseRequired(
          REASON_DECRYPTION, GetKeyDerivationParamsForPendingKeys(),
          *state_.pending_keys);
      break;
    case NigoriSpecifics::TRUSTED_VAULT_PASSPHRASE:
      broadcasting_observer_->OnTrustedVaultKeyRequired();
      break;
  }
}

void NigoriSyncBridgeImpl::MaybeNotifyBootstrapTokenUpdated() const {
  switch (state_.passphrase_type) {
    case NigoriSpecifics::UNKNOWN:
      NOTREACHED();
      return;
    case NigoriSpecifics::KEYSTORE_PASSPHRASE:
      // TODO(crbug.com/922900): notify about keystore bootstrap token updates.
      NOTIMPLEMENTED();
      return;
    case NigoriSpecifics::TRUSTED_VAULT_PASSPHRASE:
      // This may be problematic for the MIGRATION_DONE case because the local
      // keybag will be cleared and it won't be automatically recovered from
      // prefs.
      NOTIMPLEMENTED();
      return;
    case NigoriSpecifics::IMPLICIT_PASSPHRASE:
    case NigoriSpecifics::FROZEN_IMPLICIT_PASSPHRASE:
    case NigoriSpecifics::CUSTOM_PASSPHRASE:
      // |packed_custom_passphrase_key| will be empty in case serialization or
      // encryption error occurs.
      std::string packed_custom_passphrase_key =
          PackExplicitPassphraseKey(*encryptor_, *state_.cryptographer);
      if (!packed_custom_passphrase_key.empty()) {
        broadcasting_observer_->OnBootstrapTokenUpdated(
            packed_custom_passphrase_key, PASSPHRASE_BOOTSTRAP_TOKEN);
      }
  }
}

sync_pb::NigoriLocalData NigoriSyncBridgeImpl::SerializeAsNigoriLocalData()
    const {
  sync_pb::NigoriLocalData output;

  // Serialize the metadata.
  const NigoriMetadataBatch metadata_batch = processor_->GetMetadata();
  *output.mutable_model_type_state() = metadata_batch.model_type_state;
  if (metadata_batch.entity_metadata) {
    *output.mutable_entity_metadata() = *metadata_batch.entity_metadata;
  }

  // Serialize the data.
  *output.mutable_nigori_model() = state_.ToLocalProto();

  return output;
}

void NigoriSyncBridgeImpl::QueuePendingLocalCommit(
    std::unique_ptr<PendingLocalNigoriCommit> local_commit) {
  DCHECK(processor_->IsTrackingMetadata());

  pending_local_commit_queue_.push_back(std::move(local_commit));

  if (pending_local_commit_queue_.size() == 1) {
    // Verify that the newly-introduced commit (if first in the queue) applies
    // and if so call Put(), or otherwise issue an immediate failure.
    PutNextApplicablePendingLocalCommit();
  }
}

void NigoriSyncBridgeImpl::PutNextApplicablePendingLocalCommit() {
  while (!pending_local_commit_queue_.empty()) {
    NigoriState tmp_state = state_.Clone();
    bool success = pending_local_commit_queue_.front()->TryApply(&tmp_state);
    if (success) {
      // This particular commit applies cleanly.
      processor_->Put(GetData());
      break;
    }

    // The local change failed to apply.
    pending_local_commit_queue_.front()->OnFailure(
        broadcasting_observer_.get());
    pending_local_commit_queue_.pop_front();
  }
}

}  // namespace syncer
