// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_create_keys_operation.h"

#include <string>

#include "base/base64.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_key_manager.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_types.h"
#include "chromeos/cryptohome/homedir_methods.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/easy_unlock_client.h"
#include "chromeos/login/auth/key.h"
#include "crypto/encryptor.h"
#include "crypto/random.h"
#include "crypto/symmetric_key.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

const int kUserKeyByteSize = 16;
const int kSessionKeyByteSize = 16;

const int kEasyUnlockKeyRevision = 1;
const int kEasyUnlockKeyPrivileges =
    cryptohome::PRIV_MOUNT | cryptohome::PRIV_ADD | cryptohome::PRIV_REMOVE;

// TODO(xiyuan): Use real keys. http://crbug.com/409027
const char kTpmPubKey[] = {
    0x08, 0x02, 0x1a, 0x88, 0x02, 0x0a, 0x81, 0x02, 0x00, 0xdc, 0xfa, 0x10,
    0xff, 0xa7, 0x46, 0x65, 0xae, 0xef, 0x87, 0x09, 0x74, 0xea, 0x99, 0xb2,
    0xce, 0x54, 0x54, 0x7c, 0x67, 0xf4, 0x2a, 0xaa, 0x6d, 0xd0, 0x1a, 0x2e,
    0xd3, 0x1f, 0xd2, 0xc2, 0x42, 0xaf, 0x5d, 0x96, 0x0b, 0x1f, 0x89, 0x6e,
    0xfb, 0xa3, 0x54, 0x3d, 0x65, 0x54, 0xb7, 0xb1, 0x26, 0x87, 0xa5, 0xc6,
    0x88, 0x56, 0x8f, 0x32, 0xe0, 0x26, 0xc5, 0x32, 0xd2, 0x59, 0x93, 0xb9,
    0x7a, 0x7c, 0x28, 0x42, 0xec, 0x2b, 0x8e, 0x12, 0x35, 0xee, 0xe2, 0x41,
    0x4d, 0x25, 0x80, 0x6c, 0x6f, 0xba, 0xe4, 0x38, 0x95, 0x4e, 0xba, 0x9d,
    0x27, 0x55, 0xdf, 0xfe, 0xeb, 0x1b, 0x47, 0x70, 0x09, 0x57, 0x81, 0x5a,
    0x8a, 0x23, 0x3f, 0x97, 0xb1, 0xa2, 0xc7, 0x14, 0xb3, 0xe2, 0xbe, 0x2e,
    0x42, 0xd8, 0xbe, 0x30, 0xb1, 0x96, 0x15, 0x82, 0xea, 0x99, 0x48, 0x91,
    0x0e, 0x0c, 0x79, 0x7c, 0x50, 0xfc, 0x4b, 0xb4, 0x55, 0xf0, 0xfc, 0x45,
    0xe5, 0xe3, 0x4e, 0x63, 0x96, 0xac, 0x5b, 0x2d, 0x46, 0x23, 0x93, 0x65,
    0xc7, 0xf3, 0xda, 0xaf, 0x09, 0x09, 0x40, 0x0d, 0x61, 0xcf, 0x9e, 0x0c,
    0xa8, 0x08, 0x3e, 0xaf, 0x33, 0x5a, 0x6f, 0xce, 0xb6, 0x86, 0x3c, 0x1c,
    0xc0, 0xcf, 0x5a, 0x17, 0x1a, 0xff, 0x35, 0xd9, 0x7e, 0xcb, 0x60, 0xef,
    0x25, 0x1c, 0x7e, 0xc2, 0xc8, 0xa5, 0x88, 0x36, 0x1d, 0xc4, 0x12, 0x66,
    0xa4, 0xb7, 0xed, 0x38, 0xb0, 0x26, 0xce, 0x0d, 0x53, 0x78, 0x64, 0x49,
    0xdb, 0xb1, 0x1a, 0x06, 0xea, 0x33, 0xcc, 0xf1, 0xec, 0xa5, 0x75, 0x20,
    0x1e, 0xd1, 0xaa, 0x47, 0x3e, 0xd1, 0x18, 0x7e, 0xc1, 0xd8, 0xa7, 0x44,
    0xea, 0x34, 0x5b, 0xed, 0x7e, 0xa0, 0x0e, 0xe4, 0xe8, 0x1b, 0xba, 0x46,
    0x48, 0x60, 0x1d, 0xd5, 0x37, 0xdc, 0x91, 0x01, 0x5d, 0x31, 0xf0, 0xc2,
    0xc1, 0x10, 0x81, 0x80, 0x04
};

bool WebSafeBase64Decode(const std::string& encoded, std::string* decoded) {
  std::string adjusted_encoded = encoded;
  base::ReplaceChars(adjusted_encoded, "-", "+", &adjusted_encoded);
  base::ReplaceChars(adjusted_encoded, "_", "/", &adjusted_encoded);

  return base::Base64Decode(adjusted_encoded, decoded);
}

}  // namespace

/////////////////////////////////////////////////////////////////////////////
// EasyUnlockCreateKeysOperation::ChallengeCreator

class EasyUnlockCreateKeysOperation::ChallengeCreator {
 public:
  typedef base::Callback<void (bool success)> ChallengeCreatedCallback;
  ChallengeCreator(const std::string& user_key,
                   const std::string& session_key,
                   const std::string& tpm_pub_key,
                   EasyUnlockDeviceKeyData* device,
                   const ChallengeCreatedCallback& callback);
  ~ChallengeCreator();

  void Start();

  const std::string& user_key() const { return user_key_; }

 private:
  void OnEcKeyPairGenerated(const std::string& ec_public_key,
                            const std::string& ec_private_key);
  void OnEskGenerated(const std::string& esk);

  void GeneratePayload();
  void OnPayloadMessageGenerated(const std::string& payload_message);
  void OnPayloadGenerated(const std::string& payload);

  void OnChallengeGenerated(const std::string& challenge);

  const std::string user_key_;
  const std::string session_key_;
  const std::string tpm_pub_key_;
  EasyUnlockDeviceKeyData* device_;
  ChallengeCreatedCallback callback_;

  std::string ec_public_key_;
  std::string esk_;

  // Owned by DBusThreadManager
  chromeos::EasyUnlockClient* easy_unlock_client_;

  base::WeakPtrFactory<ChallengeCreator> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChallengeCreator);
};

EasyUnlockCreateKeysOperation::ChallengeCreator::ChallengeCreator(
    const std::string& user_key,
    const std::string& session_key,
    const std::string& tpm_pub_key,
    EasyUnlockDeviceKeyData* device,
    const ChallengeCreatedCallback& callback)
    : user_key_(user_key),
      session_key_(session_key),
      tpm_pub_key_(tpm_pub_key),
      device_(device),
      callback_(callback),
      easy_unlock_client_(
          chromeos::DBusThreadManager::Get()->GetEasyUnlockClient()),
      weak_ptr_factory_(this) {
}

EasyUnlockCreateKeysOperation::ChallengeCreator::~ChallengeCreator() {
}

void EasyUnlockCreateKeysOperation::ChallengeCreator::Start() {
  easy_unlock_client_->GenerateEcP256KeyPair(
      base::Bind(&ChallengeCreator::OnEcKeyPairGenerated,
                 weak_ptr_factory_.GetWeakPtr()));
}

void EasyUnlockCreateKeysOperation::ChallengeCreator::OnEcKeyPairGenerated(
    const std::string& ec_private_key,
    const std::string& ec_public_key) {
  if (ec_private_key.empty() || ec_public_key.empty()) {
    LOG(ERROR) << "Easy unlock failed to generate ec key pair.";
    callback_.Run(false);
    return;
  }

  std::string device_pub_key;
  if (!WebSafeBase64Decode(device_->public_key, &device_pub_key)) {
    LOG(ERROR) << "Easy unlock failed to decode device public key.";
    callback_.Run(false);
    return;
  }

  ec_public_key_ = ec_public_key;
  easy_unlock_client_->PerformECDHKeyAgreement(
      ec_private_key,
      device_pub_key,
      base::Bind(&ChallengeCreator::OnEskGenerated,
                 weak_ptr_factory_.GetWeakPtr()));
}

void EasyUnlockCreateKeysOperation::ChallengeCreator::OnEskGenerated(
    const std::string& esk) {
  if (esk.empty()) {
    LOG(ERROR) << "Easy unlock failed to generate challenge esk.";
    callback_.Run(false);
    return;
  }

  esk_ = esk;
  GeneratePayload();
}

void EasyUnlockCreateKeysOperation::ChallengeCreator::GeneratePayload() {
  // Work around to get HeaderAndBody bytes to use as challenge payload.
  EasyUnlockClient::CreateSecureMessageOptions options;
  options.key = esk_;
  // TODO(xiyuan, tbarzic): Wrap in a GenericPublicKey proto.
  options.verification_key_id = tpm_pub_key_;
  options.encryption_type = easy_unlock::kEncryptionTypeAES256CBC;
  options.signature_type = easy_unlock::kSignatureTypeHMACSHA256;

  easy_unlock_client_->CreateSecureMessage(
      session_key_,
      options,
      base::Bind(&ChallengeCreator::OnPayloadMessageGenerated,
                 weak_ptr_factory_.GetWeakPtr()));
}

void
EasyUnlockCreateKeysOperation::ChallengeCreator::OnPayloadMessageGenerated(
    const std::string& payload_message) {
  EasyUnlockClient::UnwrapSecureMessageOptions options;
  options.key = esk_;
  options.encryption_type = easy_unlock::kEncryptionTypeAES256CBC;
  options.signature_type = easy_unlock::kSignatureTypeHMACSHA256;

  easy_unlock_client_->UnwrapSecureMessage(
      payload_message,
      options,
      base::Bind(&ChallengeCreator::OnPayloadGenerated,
                 weak_ptr_factory_.GetWeakPtr()));
}

void EasyUnlockCreateKeysOperation::ChallengeCreator::OnPayloadGenerated(
    const std::string& payload) {
  if (payload.empty()) {
    LOG(ERROR) << "Easy unlock failed to generate challenge payload.";
    callback_.Run(false);
    return;
  }

  EasyUnlockClient::CreateSecureMessageOptions options;
  options.key = esk_;
  options.decryption_key_id = ec_public_key_;
  options.encryption_type = easy_unlock::kEncryptionTypeAES256CBC;
  options.signature_type = easy_unlock::kSignatureTypeHMACSHA256;

  easy_unlock_client_->CreateSecureMessage(
      payload,
      options,
      base::Bind(&ChallengeCreator::OnChallengeGenerated,
                 weak_ptr_factory_.GetWeakPtr()));
}

void EasyUnlockCreateKeysOperation::ChallengeCreator::OnChallengeGenerated(
    const std::string& challenge) {
  if (challenge.empty()) {
    LOG(ERROR) << "Easy unlock failed to generate challenge.";
    callback_.Run(false);
    return;
  }

  device_->challenge = challenge;
  callback_.Run(true);
}

/////////////////////////////////////////////////////////////////////////////
// EasyUnlockCreateKeysOperation

EasyUnlockCreateKeysOperation::EasyUnlockCreateKeysOperation(
    const UserContext& user_context,
    const EasyUnlockDeviceKeyDataList& devices,
    const CreateKeysCallback& callback)
    : user_context_(user_context),
      devices_(devices),
      callback_(callback),
      key_creation_index_(0),
      weak_ptr_factory_(this) {
  // Must have the secret and callback.
  DCHECK(!user_context_.GetKey()->GetSecret().empty());
  DCHECK(!callback_.is_null());
}

EasyUnlockCreateKeysOperation::~EasyUnlockCreateKeysOperation() {
}

void EasyUnlockCreateKeysOperation::Start() {
  key_creation_index_ = 0;
  CreateKeyForDeviceAtIndex(key_creation_index_);
}

void EasyUnlockCreateKeysOperation::CreateKeyForDeviceAtIndex(size_t index) {
  DCHECK_GE(index, 0u);
  if (index == devices_.size()) {
    callback_.Run(true);
    return;
  }

  std::string user_key;
  crypto::RandBytes(WriteInto(&user_key, kUserKeyByteSize + 1),
                    kUserKeyByteSize);

  scoped_ptr<crypto::SymmetricKey> session_key(
      crypto::SymmetricKey::GenerateRandomKey(crypto::SymmetricKey::AES,
                                              kSessionKeyByteSize * 8));

  std::string iv(kSessionKeyByteSize, ' ');
  crypto::Encryptor encryptor;
  if (!encryptor.Init(session_key.get(), crypto::Encryptor::CBC, iv)) {
    LOG(ERROR) << "Easy unlock failed to init encryptor for key creation.";
    callback_.Run(false);
    return;
  }

  EasyUnlockDeviceKeyData* device = &devices_[index];
  if (!encryptor.Encrypt(user_key, &device->wrapped_secret)) {
    LOG(ERROR) << "Easy unlock failed to encrypt user key for key creation.";
    callback_.Run(false);
    return;
  }

  std::string raw_session_key;
  session_key->GetRawKey(&raw_session_key);

  challenge_creator_.reset(new ChallengeCreator(
      user_key,
      raw_session_key,
      std::string(kTpmPubKey, arraysize(kTpmPubKey)),
      device,
      base::Bind(&EasyUnlockCreateKeysOperation::OnChallengeCreated,
                 weak_ptr_factory_.GetWeakPtr(),
                 index)));
  challenge_creator_->Start();
}

void EasyUnlockCreateKeysOperation::OnChallengeCreated(size_t index,
                                                       bool success) {
  DCHECK_EQ(key_creation_index_, index);

  if (!success) {
    LOG(ERROR) << "Easy unlock failed to create challenge for key creation.";
    callback_.Run(false);
    return;
  }

  SystemSaltGetter::Get()->GetSystemSalt(
      base::Bind(&EasyUnlockCreateKeysOperation::OnGetSystemSalt,
                 weak_ptr_factory_.GetWeakPtr(),
                 index));
}

void EasyUnlockCreateKeysOperation::OnGetSystemSalt(
    size_t index,
    const std::string& system_salt) {
  DCHECK_EQ(key_creation_index_, index);
  if (system_salt.empty()) {
    LOG(ERROR) << "Easy unlock failed to get system salt for key creation.";
    callback_.Run(false);
    return;
  }

  Key user_key(challenge_creator_->user_key());
  user_key.Transform(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF, system_salt);

  EasyUnlockDeviceKeyData* device = &devices_[index];
  cryptohome::KeyDefinition key_def(
      user_key.GetSecret(),
      EasyUnlockKeyManager::GetKeyLabel(index),
      kEasyUnlockKeyPrivileges);
  key_def.revision = kEasyUnlockKeyRevision;
  key_def.provider_data.push_back(cryptohome::KeyDefinition::ProviderData(
      kEasyUnlockKeyMetaNameBluetoothAddress, device->bluetooth_address));
  key_def.provider_data.push_back(cryptohome::KeyDefinition::ProviderData(
      kEasyUnlockKeyMetaNamePsk, device->psk));
  key_def.provider_data.push_back(cryptohome::KeyDefinition::ProviderData(
      kEasyUnlockKeyMetaNamePubKey, device->public_key));
  key_def.provider_data.push_back(cryptohome::KeyDefinition::ProviderData(
      kEasyUnlockKeyMetaNameChallenge, device->challenge));
  key_def.provider_data.push_back(cryptohome::KeyDefinition::ProviderData(
      kEasyUnlockKeyMetaNameWrappedSecret, device->wrapped_secret));

  // Add cryptohome key.
  std::string canonicalized =
      gaia::CanonicalizeEmail(user_context_.GetUserID());
  cryptohome::Identification id(canonicalized);

  scoped_ptr<Key> auth_key(new Key(*user_context_.GetKey()));
  if (auth_key->GetKeyType() == Key::KEY_TYPE_PASSWORD_PLAIN)
    auth_key->Transform(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF, system_salt);

  cryptohome::Authorization auth(auth_key->GetSecret(), auth_key->GetLabel());
  cryptohome::HomedirMethods::GetInstance()->AddKeyEx(
      id,
      auth,
      key_def,
      true,  // clobber
      base::Bind(&EasyUnlockCreateKeysOperation::OnKeyCreated,
                 weak_ptr_factory_.GetWeakPtr(),
                 index,
                 user_key));
}

void EasyUnlockCreateKeysOperation::OnKeyCreated(
    size_t index,
    const Key& user_key,
    bool success,
    cryptohome::MountError return_code) {
  DCHECK_EQ(key_creation_index_, index);

  if (!success) {
    LOG(ERROR) << "Easy unlock failed to create key, code=" << return_code;
    callback_.Run(false);
    return;
  }

  // If the key associated with the current context changed (i.e. in the case
  // the current signin flow was Easy signin), update the user context.
  if (user_context_.GetAuthFlow() == UserContext::AUTH_FLOW_EASY_UNLOCK &&
      user_context_.GetKey()->GetLabel() ==
          EasyUnlockKeyManager::GetKeyLabel(key_creation_index_)) {
    user_context_.SetKey(user_key);
  }

  ++key_creation_index_;
  CreateKeyForDeviceAtIndex(key_creation_index_);
}

}  // namespace chromeos
