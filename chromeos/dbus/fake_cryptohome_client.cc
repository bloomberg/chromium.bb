// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_cryptohome_client.h"

#include "base/bind.h"
#include "base/message_loop.h"

namespace chromeos {

namespace {

// A fake system salt. GetSystemSalt copies it to the given buffer.
const char kStubSystemSalt[] = "stub_system_salt";

}  // namespace

FakeCryptohomeClient::FakeCryptohomeClient() {
}

FakeCryptohomeClient::~FakeCryptohomeClient() {
}

void FakeCryptohomeClient::TpmIsBeingOwned(
    const BoolDBusMethodCallback& callback) {
}

bool FakeCryptohomeClient::Unmount(bool* success) {
  return false;
}

void FakeCryptohomeClient::AsyncCheckKey(const std::string& username,
                                         const std::string& key,
                                         const AsyncMethodCallback& callback) {
}

bool FakeCryptohomeClient::InstallAttributesIsInvalid(bool* is_invalid) {
  return false;
}

void FakeCryptohomeClient::TpmAttestationGetKeyPayload(
    attestation::AttestationKeyType key_type,
    const std::string& key_name,
    const DataMethodCallback& callback) {
}

void FakeCryptohomeClient::AsyncTpmAttestationFinishCertRequest(
    const std::string& pca_response,
    attestation::AttestationKeyType key_type,
    const std::string& key_name,
    const AsyncMethodCallback& callback) {
}

void FakeCryptohomeClient::TpmIsEnabled(
    const BoolDBusMethodCallback& callback) {
}

void FakeCryptohomeClient::AsyncTpmAttestationEnroll(
    const std::string& pca_response,
    const AsyncMethodCallback& callback) {
}

void FakeCryptohomeClient::AsyncMigrateKey(
    const std::string& username,
    const std::string& from_key,
    const std::string& to_key,
    const AsyncMethodCallback& callback) {
}

void FakeCryptohomeClient::IsMounted(const BoolDBusMethodCallback& callback) {
  MessageLoop::current()->PostTask(FROM_HERE,
                                   base::Bind(callback,
                                              DBUS_METHOD_CALL_SUCCESS, true));
}

bool FakeCryptohomeClient::InstallAttributesGet(const std::string& name,
                                                std::vector<uint8>* value,
                                                bool* successful) {
  return false;
}

void FakeCryptohomeClient::AsyncMount(const std::string& username,
                                      const std::string& key, int flags,
                                      const AsyncMethodCallback& callback) {
}

void FakeCryptohomeClient::AsyncMountGuest(
    const AsyncMethodCallback& callback) {
}

bool FakeCryptohomeClient::CallTpmIsBeingOwnedAndBlock(bool* owning) {
  return false;
}

void FakeCryptohomeClient::Pkcs11IsTpmTokenReady(
    const BoolDBusMethodCallback& callback) {
}

void FakeCryptohomeClient::TpmClearStoredPassword(
    const VoidDBusMethodCallback& callback) {
}

void FakeCryptohomeClient::TpmCanAttemptOwnership(
    const VoidDBusMethodCallback& callback) {
}

bool FakeCryptohomeClient::GetSystemSalt(std::vector<uint8>* salt) {
  salt->assign(kStubSystemSalt,
               kStubSystemSalt + arraysize(kStubSystemSalt) - 1);
  return true;
}

void FakeCryptohomeClient::TpmGetPassword(
    const StringDBusMethodCallback& callback) {
}

bool FakeCryptohomeClient::InstallAttributesFinalize(bool* successful) {
  return false;
}

void FakeCryptohomeClient::SetAsyncCallStatusHandlers(
    const AsyncCallStatusHandler& handler,
    const AsyncCallStatusWithDataHandler& data_handler) {
}

bool FakeCryptohomeClient::CallTpmIsEnabledAndBlock(bool* enabled) {
  return false;
}

bool FakeCryptohomeClient::InstallAttributesSet(
    const std::string& name,
    const std::vector<uint8>& value,
    bool* successful) {
  return false;
}

bool FakeCryptohomeClient::InstallAttributesIsFirstInstall(
    bool* is_first_install) {
  return false;
}

void FakeCryptohomeClient::TpmAttestationGetCertificate(
    attestation::AttestationKeyType key_type,
    const std::string& key_name,
    const DataMethodCallback& callback) {
}

void FakeCryptohomeClient::InstallAttributesIsReady(
    const BoolDBusMethodCallback& callback) {
  MessageLoop::current()->PostTask(FROM_HERE,
                                   base::Bind(callback,
                                              DBUS_METHOD_CALL_SUCCESS, true));
}

void FakeCryptohomeClient::TpmAttestationGetPublicKey(
    attestation::AttestationKeyType key_type,
    const std::string& key_name,
    const DataMethodCallback& callback) {
}

void FakeCryptohomeClient::TpmAttestationSignSimpleChallenge(
    attestation::AttestationKeyType key_type,
    const std::string& key_name,
    const std::string& challenge,
    const AsyncMethodCallback& callback) {
}

void FakeCryptohomeClient::Pkcs11GetTpmTokenInfo(
    const Pkcs11GetTpmTokenInfoCallback& callback) {
}

void FakeCryptohomeClient::TpmIsOwned(const BoolDBusMethodCallback& callback) {
}

void FakeCryptohomeClient::TpmAttestationIsPrepared(
    const BoolDBusMethodCallback& callback) {
}

void FakeCryptohomeClient::TpmIsReady(const BoolDBusMethodCallback& callback) {
}

void FakeCryptohomeClient::AsyncTpmAttestationCreateEnrollRequest(
    const AsyncMethodCallback& callback) {
}

void FakeCryptohomeClient::ResetAsyncCallStatusHandlers() {
}

void FakeCryptohomeClient::TpmAttestationDoesKeyExist(
    attestation::AttestationKeyType key_type,
    const std::string& key_name,
    const BoolDBusMethodCallback& callback) {
}

bool FakeCryptohomeClient::CallTpmIsOwnedAndBlock(bool* owned) {
  return false;
}

void FakeCryptohomeClient::AsyncRemove(const std::string& username,
                                       const AsyncMethodCallback& callback) {
}

void FakeCryptohomeClient::TpmAttestationSetKeyPayload(
    attestation::AttestationKeyType key_type,
    const std::string& key_name,
    const std::string& payload,
    const BoolDBusMethodCallback& callback) {
}

void FakeCryptohomeClient::GetSanitizedUsername(
    const std::string& username,
    const StringDBusMethodCallback& callback) {
}

void FakeCryptohomeClient::TpmAttestationSignEnterpriseChallenge(
    attestation::AttestationKeyType key_type,
    const std::string& key_name,
    const std::string& domain,
    const std::string& device_id,
    attestation::AttestationChallengeOptions options,
    const std::string& challenge,
    const AsyncMethodCallback& callback) {
}

void FakeCryptohomeClient::TpmAttestationIsEnrolled(
    const BoolDBusMethodCallback& callback) {
}

void FakeCryptohomeClient::TpmAttestationRegisterKey(
    attestation::AttestationKeyType key_type,
    const std::string& key_name,
    const AsyncMethodCallback& callback) {
}

bool FakeCryptohomeClient::CallTpmClearStoredPasswordAndBlock() {
  return false;
}

void FakeCryptohomeClient::AsyncTpmAttestationCreateCertRequest(
    int options,
    const AsyncMethodCallback& callback) {
}

} // namespace chromeos
