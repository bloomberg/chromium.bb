// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/nigori/nigori_state.h"

#include "base/base64.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/sync_encryption_handler.h"
#include "components/sync/nigori/cryptographer_impl.h"
#include "components/sync/protocol/nigori_local_data.pb.h"

namespace syncer {

namespace {

std::string ComputePbkdf2KeyName(const std::string& password) {
  std::string key_name;
  Nigori::CreateByDerivation(KeyDerivationParams::CreateForPbkdf2(), password)
      ->Permute(Nigori::Password, kNigoriKeyName, &key_name);
  return key_name;
}

sync_pb::CustomPassphraseKeyDerivationParams
CustomPassphraseKeyDerivationParamsToProto(const KeyDerivationParams& params) {
  sync_pb::CustomPassphraseKeyDerivationParams output;
  output.set_custom_passphrase_key_derivation_method(
      EnumKeyDerivationMethodToProto(params.method()));
  if (params.method() == KeyDerivationMethod::SCRYPT_8192_8_11) {
    output.set_custom_passphrase_key_derivation_salt(params.scrypt_salt());
  }
  return output;
}

KeyDerivationParams CustomPassphraseKeyDerivationParamsFromProto(
    const sync_pb::CustomPassphraseKeyDerivationParams& proto) {
  switch (ProtoKeyDerivationMethodToEnum(
      proto.custom_passphrase_key_derivation_method())) {
    case KeyDerivationMethod::PBKDF2_HMAC_SHA1_1003:
      return KeyDerivationParams::CreateForPbkdf2();
    case KeyDerivationMethod::SCRYPT_8192_8_11:
      return KeyDerivationParams::CreateForScrypt(
          proto.custom_passphrase_key_derivation_salt());
    case KeyDerivationMethod::UNSUPPORTED:
      break;
  }
  NOTREACHED();
  return KeyDerivationParams::CreateWithUnsupportedMethod();
}

// |encrypted| must not be null.
bool EncryptKeyBag(const CryptographerImpl& cryptographer,
                   sync_pb::EncryptedData* encrypted) {
  DCHECK(encrypted);
  DCHECK(cryptographer.CanEncrypt());

  sync_pb::CryptographerData proto = cryptographer.ToProto();
  DCHECK(!proto.key_bag().key().empty());

  // Encrypt the bag with the default Nigori.
  return cryptographer.Encrypt(proto.key_bag(), encrypted);
}

// Updates |specifics|'s individual datatypes encryption state based on
// |encrypted_types|.
void UpdateNigoriSpecificsFromEncryptedTypes(
    ModelTypeSet encrypted_types,
    sync_pb::NigoriSpecifics* specifics) {
  static_assert(41 == ModelType::NUM_ENTRIES,
                "If adding an encryptable type, update handling below.");
  specifics->set_encrypt_bookmarks(encrypted_types.Has(BOOKMARKS));
  specifics->set_encrypt_preferences(encrypted_types.Has(PREFERENCES));
  specifics->set_encrypt_autofill_profile(
      encrypted_types.Has(AUTOFILL_PROFILE));
  specifics->set_encrypt_autofill(encrypted_types.Has(AUTOFILL));
  specifics->set_encrypt_autofill_wallet_metadata(
      encrypted_types.Has(AUTOFILL_WALLET_METADATA));
  specifics->set_encrypt_themes(encrypted_types.Has(THEMES));
  specifics->set_encrypt_typed_urls(encrypted_types.Has(TYPED_URLS));
  specifics->set_encrypt_extensions(encrypted_types.Has(EXTENSIONS));
  specifics->set_encrypt_search_engines(encrypted_types.Has(SEARCH_ENGINES));
  specifics->set_encrypt_sessions(encrypted_types.Has(SESSIONS));
  specifics->set_encrypt_apps(encrypted_types.Has(APPS));
  specifics->set_encrypt_app_settings(encrypted_types.Has(APP_SETTINGS));
  specifics->set_encrypt_extension_settings(
      encrypted_types.Has(EXTENSION_SETTINGS));
  specifics->set_encrypt_dictionary(encrypted_types.Has(DICTIONARY));
  specifics->set_encrypt_favicon_images(encrypted_types.Has(FAVICON_IMAGES));
  specifics->set_encrypt_favicon_tracking(
      encrypted_types.Has(FAVICON_TRACKING));
  specifics->set_encrypt_app_list(encrypted_types.Has(APP_LIST));
  specifics->set_encrypt_arc_package(encrypted_types.Has(ARC_PACKAGE));
  specifics->set_encrypt_printers(encrypted_types.Has(PRINTERS));
  specifics->set_encrypt_reading_list(encrypted_types.Has(READING_LIST));
  specifics->set_encrypt_mountain_shares(encrypted_types.Has(MOUNTAIN_SHARES));
  specifics->set_encrypt_send_tab_to_self(
      encrypted_types.Has(SEND_TAB_TO_SELF));
  specifics->set_encrypt_web_apps(encrypted_types.Has(WEB_APPS));
  specifics->set_encrypt_os_preferences(encrypted_types.Has(OS_PREFERENCES));
}

void UpdateSpecificsFromKeyDerivationParams(
    const KeyDerivationParams& params,
    sync_pb::NigoriSpecifics* specifics) {
  DCHECK_EQ(specifics->passphrase_type(),
            sync_pb::NigoriSpecifics::CUSTOM_PASSPHRASE);
  DCHECK_NE(params.method(), KeyDerivationMethod::UNSUPPORTED);
  specifics->set_custom_passphrase_key_derivation_method(
      EnumKeyDerivationMethodToProto(params.method()));
  if (params.method() == KeyDerivationMethod::SCRYPT_8192_8_11) {
    // Persist the salt used for key derivation in Nigori if we're using scrypt.
    std::string encoded_salt;
    base::Base64Encode(params.scrypt_salt(), &encoded_salt);
    specifics->set_custom_passphrase_key_derivation_salt(encoded_salt);
  }
}

bool EncryptKeystoreDecryptorToken(
    const CryptographerImpl& cryptographer,
    sync_pb::EncryptedData* keystore_decryptor_token,
    const std::vector<std::string>& keystore_keys) {
  DCHECK(keystore_decryptor_token);

  const sync_pb::NigoriKey default_key = cryptographer.ExportDefaultKey();

  std::unique_ptr<Cryptographer> keystore_cryptographer =
      CreateCryptographerFromKeystoreKeys(keystore_keys);
  if (!keystore_cryptographer) {
    return false;
  }

  return keystore_cryptographer->EncryptString(default_key.SerializeAsString(),
                                               keystore_decryptor_token);
}

}  // namespace

std::unique_ptr<CryptographerImpl> CreateCryptographerFromKeystoreKeys(
    const std::vector<std::string>& keystore_keys) {
  std::unique_ptr<CryptographerImpl> cryptographer =
      CryptographerImpl::CreateEmpty();

  if (keystore_keys.empty()) {
    return cryptographer;
  }

  std::string last_key_name;
  for (const std::string& key : keystore_keys) {
    last_key_name =
        cryptographer->EmplaceKey(key, KeyDerivationParams::CreateForPbkdf2());
    // TODO(crbug.com/922900): possible behavioral change. Old implementation
    // fails only if we failed to add current keystore key. Failing to add any
    // of these keys doesn't seem valid. This line seems to be a good candidate
    // for UMA, as it's not a normal situation, if we fail to add any key.
    if (last_key_name.empty()) {
      return nullptr;
    }
  }

  DCHECK(!last_key_name.empty());
  cryptographer->SelectDefaultEncryptionKey(last_key_name);

  return cryptographer;
}

// static
NigoriState NigoriState::CreateFromLocalProto(
    const sync_pb::NigoriModel& proto) {
  NigoriState state;

  state.cryptographer =
      CryptographerImpl::FromProto(proto.cryptographer_data());

  if (proto.has_pending_keys()) {
    state.pending_keys = proto.pending_keys();
  }

  state.passphrase_type = proto.passphrase_type();
  state.keystore_migration_time =
      ProtoTimeToTime(proto.keystore_migration_time());
  state.custom_passphrase_time =
      ProtoTimeToTime(proto.custom_passphrase_time());
  if (proto.has_custom_passphrase_key_derivation_params()) {
    state.custom_passphrase_key_derivation_params =
        CustomPassphraseKeyDerivationParamsFromProto(
            proto.custom_passphrase_key_derivation_params());
  }
  state.encrypt_everything = proto.encrypt_everything();
  for (int i = 0; i < proto.keystore_key_size(); ++i) {
    state.keystore_keys.push_back(proto.keystore_key(i));
  }
  if (proto.has_pending_keystore_decryptor_token()) {
    state.pending_keystore_decryptor_token =
        proto.pending_keystore_decryptor_token();
  }
  return state;
}

NigoriState::NigoriState()
    : cryptographer(CryptographerImpl::CreateEmpty()),
      passphrase_type(kInitialPassphraseType),
      encrypt_everything(kInitialEncryptEverything) {}

NigoriState::NigoriState(NigoriState&& other) = default;

NigoriState::~NigoriState() = default;

NigoriState& NigoriState::operator=(NigoriState&& other) = default;

sync_pb::NigoriModel NigoriState::ToLocalProto() const {
  sync_pb::NigoriModel proto;
  *proto.mutable_cryptographer_data() = cryptographer->ToProto();
  if (pending_keys.has_value()) {
    *proto.mutable_pending_keys() = *pending_keys;
  }
  if (!keystore_keys.empty()) {
    proto.set_current_keystore_key_name(
        ComputePbkdf2KeyName(keystore_keys.back()));
  }
  proto.set_passphrase_type(passphrase_type);
  if (!keystore_migration_time.is_null()) {
    proto.set_keystore_migration_time(TimeToProtoTime(keystore_migration_time));
  }
  if (!custom_passphrase_time.is_null()) {
    proto.set_custom_passphrase_time(TimeToProtoTime(custom_passphrase_time));
  }
  if (custom_passphrase_key_derivation_params) {
    *proto.mutable_custom_passphrase_key_derivation_params() =
        CustomPassphraseKeyDerivationParamsToProto(
            *custom_passphrase_key_derivation_params);
  }
  proto.set_encrypt_everything(encrypt_everything);
  ModelTypeSet encrypted_types = SyncEncryptionHandler::SensitiveTypes();
  if (encrypt_everything) {
    encrypted_types = EncryptableUserTypes();
  }
  for (ModelType model_type : encrypted_types) {
    proto.add_encrypted_types_specifics_field_number(
        GetSpecificsFieldNumberFromModelType(model_type));
  }
  // TODO(crbug.com/970213): we currently store keystore keys in proto only to
  // allow rollback of USS Nigori. Having keybag with all keystore keys and
  // |current_keystore_key_name| is enough to support all logic. We should
  // remove them few milestones after USS migration completed.
  for (const std::string& keystore_key : keystore_keys) {
    proto.add_keystore_key(keystore_key);
  }
  if (pending_keystore_decryptor_token.has_value()) {
    *proto.mutable_pending_keystore_decryptor_token() =
        *pending_keystore_decryptor_token;
  }
  return proto;
}

sync_pb::NigoriSpecifics NigoriState::ToSpecificsProto() const {
  sync_pb::NigoriSpecifics specifics;
  if (cryptographer->CanEncrypt()) {
    EncryptKeyBag(*cryptographer, specifics.mutable_encryption_keybag());
  } else {
    DCHECK(pending_keys.has_value());
    // This case is reachable only from processor's GetAllNodesForDebugging(),
    // since currently commit is never issued while bridge has |pending_keys_|.
    // Note: with complete support of TRUSTED_VAULT mode, commit might be
    // issued in this case as well.
    *specifics.mutable_encryption_keybag() = *pending_keys;
  }
  specifics.set_keybag_is_frozen(true);
  specifics.set_encrypt_everything(encrypt_everything);
  if (encrypt_everything) {
    UpdateNigoriSpecificsFromEncryptedTypes(EncryptableUserTypes(), &specifics);
  }
  specifics.set_passphrase_type(passphrase_type);
  if (passphrase_type == sync_pb::NigoriSpecifics::CUSTOM_PASSPHRASE) {
    DCHECK(custom_passphrase_key_derivation_params);
    UpdateSpecificsFromKeyDerivationParams(
        *custom_passphrase_key_derivation_params, &specifics);
  }
  if (passphrase_type == sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE) {
    // TODO(crbug.com/922900): it seems possible to have corrupted
    // |pending_keystore_decryptor_token| and an ability to recover it in case
    // |pending_keys| isn't set and |keystore_keys| contains some keys.
    if (pending_keystore_decryptor_token.has_value()) {
      *specifics.mutable_keystore_decryptor_token() =
          *pending_keystore_decryptor_token;
    } else {
      DCHECK(!keystore_keys.empty());
      EncryptKeystoreDecryptorToken(
          *cryptographer, specifics.mutable_keystore_decryptor_token(),
          keystore_keys);
    }
  }
  if (!keystore_migration_time.is_null()) {
    specifics.set_keystore_migration_time(
        TimeToProtoTime(keystore_migration_time));
  }
  if (!custom_passphrase_time.is_null()) {
    specifics.set_custom_passphrase_time(
        TimeToProtoTime(custom_passphrase_time));
  }
  // TODO(crbug.com/922900): add other fields support.
  NOTIMPLEMENTED();
  return specifics;
}

}  // namespace syncer
