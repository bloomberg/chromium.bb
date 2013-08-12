// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/cryptohome_client_stub.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "crypto/nss_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

CryptohomeClientStubImpl::CryptohomeClientStubImpl()
    : async_call_id_(1),
      tpm_is_ready_counter_(0),
      locked_(false),
      weak_ptr_factory_(this) {}

CryptohomeClientStubImpl::~CryptohomeClientStubImpl() {}

void CryptohomeClientStubImpl::SetAsyncCallStatusHandlers(
    const AsyncCallStatusHandler& handler,
    const AsyncCallStatusWithDataHandler& data_handler) {
  async_call_status_handler_ = handler;
  async_call_status_data_handler_ = data_handler;
}

void CryptohomeClientStubImpl::ResetAsyncCallStatusHandlers() {
  async_call_status_handler_.Reset();
  async_call_status_data_handler_.Reset();
}

void CryptohomeClientStubImpl::IsMounted(
    const BoolDBusMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, true));
}

bool CryptohomeClientStubImpl::Unmount(bool* success) {
  *success = true;
  return true;
}

void CryptohomeClientStubImpl::AsyncCheckKey(
    const std::string& username,
    const std::string& key,
    const AsyncMethodCallback& callback) {
  ReturnAsyncMethodResult(callback, false);
}

void CryptohomeClientStubImpl::AsyncMigrateKey(
    const std::string& username,
    const std::string& from_key,
    const std::string& to_key,
    const AsyncMethodCallback& callback) {
  ReturnAsyncMethodResult(callback, false);
}

void CryptohomeClientStubImpl::AsyncRemove(
    const std::string& username,
    const AsyncMethodCallback& callback) {
  ReturnAsyncMethodResult(callback, false);
}

bool CryptohomeClientStubImpl::GetSystemSalt(std::vector<uint8>* salt) {
  const char kStubSystemSalt[] = "stub_system_salt";
  salt->assign(kStubSystemSalt,
               kStubSystemSalt + arraysize(kStubSystemSalt) - 1);
  return true;
}

void CryptohomeClientStubImpl::GetSanitizedUsername(
    const std::string& username,
    const StringDBusMethodCallback& callback) {
  // Even for stub implementation we have to return different values so that
  // multi-profiles would work.
  std::string sanitized_username = GetStubSanitizedUsername(username);
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, sanitized_username));
}

std::string CryptohomeClientStubImpl::BlockingGetSanitizedUsername(
    const std::string& username) {
  return GetStubSanitizedUsername(username);
}

void CryptohomeClientStubImpl::AsyncMount(const std::string& username,
                                          const std::string& key,
                                          int flags,
                                          const AsyncMethodCallback& callback) {
  ReturnAsyncMethodResult(callback, false);
}

void CryptohomeClientStubImpl::AsyncAddKey(
    const std::string& username,
    const std::string& key,
    const std::string& new_key,
    const AsyncMethodCallback& callback) {
  ReturnAsyncMethodResult(callback, false);
}

void CryptohomeClientStubImpl::AsyncMountGuest(
    const AsyncMethodCallback& callback) {
  ReturnAsyncMethodResult(callback, false);
}

void CryptohomeClientStubImpl::AsyncMountPublic(
    const std::string& public_mount_id,
    int flags,
    const AsyncMethodCallback& callback) {
  ReturnAsyncMethodResult(callback, false);
}

void CryptohomeClientStubImpl::TpmIsReady(
    const BoolDBusMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, true));
}

void CryptohomeClientStubImpl::TpmIsEnabled(
    const BoolDBusMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, true));
}

bool CryptohomeClientStubImpl::CallTpmIsEnabledAndBlock(bool* enabled) {
  *enabled = true;
  return true;
}

void CryptohomeClientStubImpl::TpmGetPassword(
    const StringDBusMethodCallback& callback) {
  const char kStubTpmPassword[] = "Stub-TPM-password";
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, kStubTpmPassword));
}

void CryptohomeClientStubImpl::TpmIsOwned(
    const BoolDBusMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, true));
}

bool CryptohomeClientStubImpl::CallTpmIsOwnedAndBlock(bool* owned) {
  *owned = true;
  return true;
}

void CryptohomeClientStubImpl::TpmIsBeingOwned(
    const BoolDBusMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, true));
}

bool CryptohomeClientStubImpl::CallTpmIsBeingOwnedAndBlock(bool* owning) {
  *owning = true;
  return true;
}

void CryptohomeClientStubImpl::TpmCanAttemptOwnership(
    const VoidDBusMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS));
}

void CryptohomeClientStubImpl::TpmClearStoredPassword(
    const VoidDBusMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS));
}

bool CryptohomeClientStubImpl::CallTpmClearStoredPasswordAndBlock() {
  return true;
}

void CryptohomeClientStubImpl::Pkcs11IsTpmTokenReady(
    const BoolDBusMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, true));
}

void CryptohomeClientStubImpl::Pkcs11GetTpmTokenInfo(
    const Pkcs11GetTpmTokenInfoCallback& callback) {
  const char kStubUserPin[] = "012345";
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback,
                 DBUS_METHOD_CALL_SUCCESS,
                 std::string(crypto::kTestTPMTokenName),
                 std::string(kStubUserPin)));
}

bool CryptohomeClientStubImpl::InstallAttributesGet(const std::string& name,
                                                    std::vector<uint8>* value,
                                                    bool* successful) {
  if (install_attrs_.find(name) != install_attrs_.end()) {
    *value = install_attrs_[name];
    *successful = true;
  } else {
    value->clear();
    *successful = false;
  }
  return true;
}

bool CryptohomeClientStubImpl::InstallAttributesSet(
    const std::string& name,
    const std::vector<uint8>& value,
    bool* successful) {
  install_attrs_[name] = value;
  *successful = true;
  return true;
}

bool CryptohomeClientStubImpl::InstallAttributesFinalize(bool* successful) {
  locked_ = true;
  *successful = true;
  return true;
}

void CryptohomeClientStubImpl::InstallAttributesIsReady(
    const BoolDBusMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, true));
}

bool CryptohomeClientStubImpl::InstallAttributesIsInvalid(bool* is_invalid) {
  *is_invalid = false;
  return true;
}

bool CryptohomeClientStubImpl::InstallAttributesIsFirstInstall(
    bool* is_first_install) {
  *is_first_install = !locked_;
  return true;
}

void CryptohomeClientStubImpl::TpmAttestationIsPrepared(
    const BoolDBusMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, true));
}

void CryptohomeClientStubImpl::TpmAttestationIsEnrolled(
    const BoolDBusMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, true));
}

void CryptohomeClientStubImpl::AsyncTpmAttestationCreateEnrollRequest(
    const AsyncMethodCallback& callback) {
  ReturnAsyncMethodResult(callback, true);
}

void CryptohomeClientStubImpl::AsyncTpmAttestationEnroll(
    const std::string& pca_response,
    const AsyncMethodCallback& callback) {
  ReturnAsyncMethodResult(callback, false);
}

void CryptohomeClientStubImpl::AsyncTpmAttestationCreateCertRequest(
    int options,
    const AsyncMethodCallback& callback) {
  ReturnAsyncMethodResult(callback, true);
}

void CryptohomeClientStubImpl::AsyncTpmAttestationFinishCertRequest(
    const std::string& pca_response,
    attestation::AttestationKeyType key_type,
    const std::string& key_name,
    const AsyncMethodCallback& callback) {
  ReturnAsyncMethodResult(callback, true);
}

void CryptohomeClientStubImpl::TpmAttestationDoesKeyExist(
    attestation::AttestationKeyType key_type,
    const std::string& key_name,
    const BoolDBusMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, false));
}

void CryptohomeClientStubImpl::TpmAttestationGetCertificate(
    attestation::AttestationKeyType key_type,
    const std::string& key_name,
    const DataMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, false, std::string()));
}

void CryptohomeClientStubImpl::TpmAttestationGetPublicKey(
    attestation::AttestationKeyType key_type,
    const std::string& key_name,
    const DataMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, false, std::string()));
}

void CryptohomeClientStubImpl::TpmAttestationRegisterKey(
    attestation::AttestationKeyType key_type,
    const std::string& key_name,
    const AsyncMethodCallback& callback) {
  ReturnAsyncMethodResult(callback, true);
}

void CryptohomeClientStubImpl::TpmAttestationSignEnterpriseChallenge(
    attestation::AttestationKeyType key_type,
    const std::string& key_name,
    const std::string& domain,
    const std::string& device_id,
    attestation::AttestationChallengeOptions options,
    const std::string& challenge,
    const AsyncMethodCallback& callback) {
  ReturnAsyncMethodResult(callback, true);
}

void CryptohomeClientStubImpl::TpmAttestationSignSimpleChallenge(
    attestation::AttestationKeyType key_type,
    const std::string& key_name,
    const std::string& challenge,
    const AsyncMethodCallback& callback) {
  ReturnAsyncMethodResult(callback, true);
}

void CryptohomeClientStubImpl::TpmAttestationGetKeyPayload(
    attestation::AttestationKeyType key_type,
    const std::string& key_name,
    const DataMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, false, std::string()));
}

void CryptohomeClientStubImpl::TpmAttestationSetKeyPayload(
    attestation::AttestationKeyType key_type,
    const std::string& key_name,
    const std::string& payload,
    const BoolDBusMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, false));
}

void CryptohomeClientStubImpl::ReturnAsyncMethodResult(
    const AsyncMethodCallback& callback,
    bool returns_data) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&CryptohomeClientStubImpl::ReturnAsyncMethodResultInternal,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback,
                 returns_data));
}

void CryptohomeClientStubImpl::ReturnAsyncMethodResultInternal(
    const AsyncMethodCallback& callback,
    bool returns_data) {
  callback.Run(async_call_id_);
  if (!returns_data && !async_call_status_handler_.is_null()) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(async_call_status_handler_,
                   async_call_id_,
                   true,
                   cryptohome::MOUNT_ERROR_NONE));
  } else if (returns_data && !async_call_status_data_handler_.is_null()) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(async_call_status_data_handler_,
                   async_call_id_,
                   true,
                   std::string()));
  }
  ++async_call_id_;
}

}  // namespace chromeos
