// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_group_private_key_sharer_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/services/device_sync/cryptauth_client.h"
#include "chromeos/services/device_sync/cryptauth_ecies_encryptor_impl.h"
#include "chromeos/services/device_sync/cryptauth_key.h"
#include "crypto/sha2.h"

namespace chromeos {

namespace device_sync {

namespace {

// Timeout values for asynchronous operations.
// TODO(https://crbug.com/933656): Tune these values.
constexpr base::TimeDelta kWaitingForGroupPrivateKeyEncryptionTimeout =
    base::TimeDelta::FromSeconds(10);
constexpr base::TimeDelta kWaitingForShareGroupPrivateKeyResponseTimeout =
    base::TimeDelta::FromSeconds(10);

CryptAuthDeviceSyncResult::ResultCode
ShareGroupPrivateKeyNetworkRequestErrorToResultCode(NetworkRequestError error) {
  switch (error) {
    case NetworkRequestError::kOffline:
      return CryptAuthDeviceSyncResult::ResultCode::
          kErrorShareGroupPrivateKeyApiCallOffline;
    case NetworkRequestError::kEndpointNotFound:
      return CryptAuthDeviceSyncResult::ResultCode::
          kErrorShareGroupPrivateKeyApiCallEndpointNotFound;
    case NetworkRequestError::kAuthenticationError:
      return CryptAuthDeviceSyncResult::ResultCode::
          kErrorShareGroupPrivateKeyApiCallAuthenticationError;
    case NetworkRequestError::kBadRequest:
      return CryptAuthDeviceSyncResult::ResultCode::
          kErrorShareGroupPrivateKeyApiCallBadRequest;
    case NetworkRequestError::kResponseMalformed:
      return CryptAuthDeviceSyncResult::ResultCode::
          kErrorShareGroupPrivateKeyApiCallResponseMalformed;
    case NetworkRequestError::kInternalServerError:
      return CryptAuthDeviceSyncResult::ResultCode::
          kErrorShareGroupPrivateKeyApiCallInternalServerError;
    case NetworkRequestError::kUnknown:
      return CryptAuthDeviceSyncResult::ResultCode::
          kErrorShareGroupPrivateKeyApiCallUnknownError;
  }
}

int64_t CalculateInt64Sha256Hash(const std::string& str) {
  int64_t hash;
  crypto::SHA256HashString(str, &hash, sizeof(int64_t));
  return hash;
}

}  // namespace

// static
CryptAuthGroupPrivateKeySharerImpl::Factory*
    CryptAuthGroupPrivateKeySharerImpl::Factory::test_factory_ = nullptr;

// static
CryptAuthGroupPrivateKeySharerImpl::Factory*
CryptAuthGroupPrivateKeySharerImpl::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<CryptAuthGroupPrivateKeySharerImpl::Factory>
      factory;
  return factory.get();
}

// static
void CryptAuthGroupPrivateKeySharerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

CryptAuthGroupPrivateKeySharerImpl::Factory::~Factory() = default;

std::unique_ptr<CryptAuthGroupPrivateKeySharer>
CryptAuthGroupPrivateKeySharerImpl::Factory::BuildInstance(
    CryptAuthClientFactory* client_factory,
    std::unique_ptr<base::OneShotTimer> timer) {
  return base::WrapUnique(
      new CryptAuthGroupPrivateKeySharerImpl(client_factory, std::move(timer)));
}

CryptAuthGroupPrivateKeySharerImpl::CryptAuthGroupPrivateKeySharerImpl(
    CryptAuthClientFactory* client_factory,
    std::unique_ptr<base::OneShotTimer> timer)
    : client_factory_(client_factory), timer_(std::move(timer)) {
  DCHECK(client_factory);
}

CryptAuthGroupPrivateKeySharerImpl::~CryptAuthGroupPrivateKeySharerImpl() =
    default;

// static
base::Optional<base::TimeDelta>
CryptAuthGroupPrivateKeySharerImpl::GetTimeoutForState(State state) {
  switch (state) {
    case State::kWaitingForGroupPrivateKeyEncryption:
      return kWaitingForGroupPrivateKeyEncryptionTimeout;
    case State::kWaitingForShareGroupPrivateKeyResponse:
      return kWaitingForShareGroupPrivateKeyResponseTimeout;
    default:
      // Signifies that there should not be a timeout.
      return base::nullopt;
  }
}

// static
base::Optional<CryptAuthDeviceSyncResult::ResultCode>
CryptAuthGroupPrivateKeySharerImpl::ResultCodeErrorFromTimeoutDuringState(
    State state) {
  switch (state) {
    case State::kWaitingForGroupPrivateKeyEncryption:
      return CryptAuthDeviceSyncResult::ResultCode::
          kErrorTimeoutWaitingForGroupPrivateKeyEncryption;
    case State::kWaitingForShareGroupPrivateKeyResponse:
      return CryptAuthDeviceSyncResult::ResultCode::
          kErrorTimeoutWaitingForShareGroupPrivateKeyResponse;
    default:
      return base::nullopt;
  }
}

void CryptAuthGroupPrivateKeySharerImpl::SetState(State state) {
  timer_->Stop();

  PA_LOG(INFO) << "Transitioning from " << state_ << " to " << state;
  state_ = state;

  base::Optional<base::TimeDelta> timeout_for_state = GetTimeoutForState(state);
  if (!timeout_for_state)
    return;

  // TODO(https://crbug.com/936273): Add metrics to track failure rates due to
  // async timeouts.
  timer_->Start(FROM_HERE, *timeout_for_state,
                base::BindOnce(&CryptAuthGroupPrivateKeySharerImpl::OnTimeout,
                               base::Unretained(this)));
}

void CryptAuthGroupPrivateKeySharerImpl::OnTimeout() {
  // If there's a timeout specified, there should be a corresponding error code.
  base::Optional<CryptAuthDeviceSyncResult::ResultCode> error_code =
      ResultCodeErrorFromTimeoutDuringState(state_);
  DCHECK(error_code);

  FinishAttempt(*error_code);
}

void CryptAuthGroupPrivateKeySharerImpl::OnAttemptStarted(
    const cryptauthv2::RequestContext& request_context,
    const CryptAuthKey& group_key,
    const IdToEncryptingKeyMap& id_to_encrypting_key_map) {
  DCHECK_EQ(State::kNotStarted, state_);
  DCHECK(!group_key.private_key().empty());

  CryptAuthEciesEncryptor::IdToInputMap group_private_keys_to_encrypt;
  for (const auto& id_encrypting_key_pair : id_to_encrypting_key_map) {
    const std::string& id = id_encrypting_key_pair.first;
    const std::string& encrypting_key = id_encrypting_key_pair.second;

    // If the encrypting key is empty, the group private key cannot be
    // encrypted. Skip this ID and attempt to encrypt the group private key for
    // as many IDs as possible.
    // TODO(https://crbug.com/936273): Add metrics for empty device public keys.
    if (encrypting_key.empty()) {
      PA_LOG(ERROR) << "Cannot encrypt group private key for device with ID "
                    << id << ". Encrypting key is empty.";
      did_non_fatal_error_occur_ = true;
      continue;
    }

    group_private_keys_to_encrypt[id] = CryptAuthEciesEncryptor::PayloadAndKey(
        group_key.private_key(), encrypting_key);
  }

  // All encrypting keys are empty; encryption not possible.
  if (group_private_keys_to_encrypt.empty()) {
    FinishAttempt(
        CryptAuthDeviceSyncResult::ResultCode::kErrorEncryptingGroupPrivateKey);
    return;
  }

  SetState(State::kWaitingForGroupPrivateKeyEncryption);

  encryptor_ = CryptAuthEciesEncryptorImpl::Factory::Get()->BuildInstance();
  encryptor_->BatchEncrypt(
      group_private_keys_to_encrypt,
      base::BindOnce(
          &CryptAuthGroupPrivateKeySharerImpl::OnGroupPrivateKeysEncrypted,
          base::Unretained(this), request_context, group_key));
}

void CryptAuthGroupPrivateKeySharerImpl::OnGroupPrivateKeysEncrypted(
    const cryptauthv2::RequestContext& request_context,
    const CryptAuthKey& group_key,
    const CryptAuthEciesEncryptor::IdToOutputMap&
        id_to_encrypted_group_private_key_map) {
  DCHECK_EQ(State::kWaitingForGroupPrivateKeyEncryption, state_);

  cryptauthv2::ShareGroupPrivateKeyRequest request;
  request.mutable_context()->CopyFrom(request_context);

  for (const auto& id_encrypted_key_pair :
       id_to_encrypted_group_private_key_map) {
    // If the group private key could not be encrypted for this ID--due to an
    // invalid encrypting key, for instance--skip it. Continue to share as many
    // encrypted group private keys as possible.
    // TODO(https://crbug.com/936273): Add metrics for group private key
    // encryption failures.
    bool was_encryption_successful = id_encrypted_key_pair.second.has_value();
    if (!was_encryption_successful) {
      PA_LOG(ERROR) << "Group private key could not be encrypted for device "
                    << "with ID " << id_encrypted_key_pair.first;
      did_non_fatal_error_occur_ = true;
      continue;
    }

    cryptauthv2::EncryptedGroupPrivateKey* encrypted_key =
        request.add_encrypted_group_private_keys();
    encrypted_key->set_recipient_device_id(id_encrypted_key_pair.first);
    encrypted_key->set_sender_device_id(request_context.device_id());
    encrypted_key->set_encrypted_private_key(*id_encrypted_key_pair.second);

    // CryptAuth requires a SHA-256 hash of the group public key as an int64.
    encrypted_key->set_group_public_key_hash(
        CalculateInt64Sha256Hash(group_key.public_key()));
  }

  // All encryption attempts failed; nothing to share.
  if (request.encrypted_group_private_keys().empty()) {
    FinishAttempt(
        CryptAuthDeviceSyncResult::ResultCode::kErrorEncryptingGroupPrivateKey);
    return;
  }

  SetState(State::kWaitingForShareGroupPrivateKeyResponse);

  cryptauth_client_ = client_factory_->CreateInstance();
  cryptauth_client_->ShareGroupPrivateKey(
      request,
      base::Bind(
          &CryptAuthGroupPrivateKeySharerImpl::OnShareGroupPrivateKeySuccess,
          base::Unretained(this)),
      base::Bind(
          &CryptAuthGroupPrivateKeySharerImpl::OnShareGroupPrivateKeyFailure,
          base::Unretained(this)));
}

void CryptAuthGroupPrivateKeySharerImpl::OnShareGroupPrivateKeySuccess(
    const cryptauthv2::ShareGroupPrivateKeyResponse& response) {
  DCHECK_EQ(State::kWaitingForShareGroupPrivateKeyResponse, state_);

  CryptAuthDeviceSyncResult::ResultCode result_code =
      did_non_fatal_error_occur_
          ? CryptAuthDeviceSyncResult::ResultCode::kFinishedWithNonFatalErrors
          : CryptAuthDeviceSyncResult::ResultCode::kSuccess;
  FinishAttempt(result_code);
}

void CryptAuthGroupPrivateKeySharerImpl::OnShareGroupPrivateKeyFailure(
    NetworkRequestError error) {
  DCHECK_EQ(State::kWaitingForShareGroupPrivateKeyResponse, state_);

  FinishAttempt(ShareGroupPrivateKeyNetworkRequestErrorToResultCode(error));
}

void CryptAuthGroupPrivateKeySharerImpl::FinishAttempt(
    const CryptAuthDeviceSyncResult::ResultCode& result_code) {
  encryptor_.reset();
  cryptauth_client_.reset();

  SetState(State::kFinished);

  OnAttemptFinished(result_code);
}

std::ostream& operator<<(
    std::ostream& stream,
    const CryptAuthGroupPrivateKeySharerImpl::State& state) {
  switch (state) {
    case CryptAuthGroupPrivateKeySharerImpl::State::kNotStarted:
      stream << "[GroupPrivateKeySharer state: Not started]";
      break;
    case CryptAuthGroupPrivateKeySharerImpl::State::
        kWaitingForGroupPrivateKeyEncryption:
      stream << "[GroupPrivateKeySharer state: Waiting for group private key "
             << "encryption]";
      break;
    case CryptAuthGroupPrivateKeySharerImpl::State::
        kWaitingForShareGroupPrivateKeyResponse:
      stream << "[GroupPrivateKeySharer state: Waiting for "
             << "ShareGroupPrivateKey response]";
      break;
    case CryptAuthGroupPrivateKeySharerImpl::State::kFinished:
      stream << "[GroupPrivateKeySharer state: Finished]";
      break;
  }

  return stream;
}

}  // namespace device_sync

}  // namespace chromeos
