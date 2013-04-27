// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_CRYPTOHOME_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_CRYPTOHOME_CLIENT_H_

#include <string>

#include "chromeos/dbus/cryptohome_client.h"

namespace chromeos {

// A fake implementation of CryptohomeClient. This only calls callbacks given
// as parameters.
class FakeCryptohomeClient : public CryptohomeClient {
 public:
  FakeCryptohomeClient();
  virtual ~FakeCryptohomeClient();

  // CryptohomeClient overrides.
  virtual void SetAsyncCallStatusHandlers(
      const AsyncCallStatusHandler& handler,
      const AsyncCallStatusWithDataHandler& data_handler) OVERRIDE;
  virtual void ResetAsyncCallStatusHandlers() OVERRIDE;
  virtual void IsMounted(const BoolDBusMethodCallback& callback) OVERRIDE;
  virtual bool Unmount(bool* success) OVERRIDE;
  virtual void AsyncCheckKey(const std::string& username,
                             const std::string& key,
                             const AsyncMethodCallback& callback) OVERRIDE;
  virtual void AsyncMigrateKey(const std::string& username,
                               const std::string& from_key,
                               const std::string& to_key,
                               const AsyncMethodCallback& callback) OVERRIDE;
  virtual void AsyncRemove(const std::string& username,
                           const AsyncMethodCallback& callback) OVERRIDE;
  virtual bool GetSystemSalt(std::vector<uint8>* salt) OVERRIDE;
  virtual void GetSanitizedUsername(
      const std::string& username,
      const StringDBusMethodCallback& callback) OVERRIDE;
  virtual void AsyncMount(const std::string& username,
                          const std::string& key,
                          int flags,
                          const AsyncMethodCallback& callback) OVERRIDE;
  virtual void AsyncMountGuest(const AsyncMethodCallback& callback) OVERRIDE;
  virtual void TpmIsReady(const BoolDBusMethodCallback& callback) OVERRIDE;
  virtual void TpmIsEnabled(const BoolDBusMethodCallback& callback) OVERRIDE;
  virtual bool CallTpmIsEnabledAndBlock(bool* enabled) OVERRIDE;
  virtual void TpmGetPassword(
      const StringDBusMethodCallback& callback) OVERRIDE;
  virtual void TpmIsOwned(const BoolDBusMethodCallback& callback) OVERRIDE;
  virtual bool CallTpmIsOwnedAndBlock(bool* owned) OVERRIDE;
  virtual void TpmIsBeingOwned(const BoolDBusMethodCallback& callback) OVERRIDE;
  virtual bool CallTpmIsBeingOwnedAndBlock(bool* owning) OVERRIDE;
  virtual void TpmCanAttemptOwnership(
      const VoidDBusMethodCallback& callback) OVERRIDE;
  virtual void TpmClearStoredPassword(
      const VoidDBusMethodCallback& callback) OVERRIDE;
  virtual bool CallTpmClearStoredPasswordAndBlock() OVERRIDE;
  virtual void Pkcs11IsTpmTokenReady(
      const BoolDBusMethodCallback& callback) OVERRIDE;
  virtual void Pkcs11GetTpmTokenInfo(
      const Pkcs11GetTpmTokenInfoCallback& callback) OVERRIDE;
  virtual bool InstallAttributesGet(const std::string& name,
                                    std::vector<uint8>* value,
                                    bool* successful) OVERRIDE;
  virtual bool InstallAttributesSet(const std::string& name,
                                    const std::vector<uint8>& value,
                                    bool* successful) OVERRIDE;
  virtual bool InstallAttributesFinalize(bool* successful) OVERRIDE;
  virtual void InstallAttributesIsReady(
      const BoolDBusMethodCallback& callback) OVERRIDE;
  virtual bool InstallAttributesIsInvalid(bool* is_invalid) OVERRIDE;
  virtual bool InstallAttributesIsFirstInstall(bool* is_first_install) OVERRIDE;
  virtual void TpmAttestationIsPrepared(
        const BoolDBusMethodCallback& callback) OVERRIDE;
  virtual void TpmAttestationIsEnrolled(
        const BoolDBusMethodCallback& callback) OVERRIDE;
  virtual void AsyncTpmAttestationCreateEnrollRequest(
      const AsyncMethodCallback& callback) OVERRIDE;
  virtual void AsyncTpmAttestationEnroll(
      const std::string& pca_response,
      const AsyncMethodCallback& callback) OVERRIDE;
  virtual void AsyncTpmAttestationCreateCertRequest(
      int options,
      const AsyncMethodCallback& callback) OVERRIDE;
  virtual void AsyncTpmAttestationFinishCertRequest(
      const std::string& pca_response,
      attestation::AttestationKeyType key_type,
      const std::string& key_name,
      const AsyncMethodCallback& callback) OVERRIDE;
  virtual void TpmAttestationDoesKeyExist(
      attestation::AttestationKeyType key_type,
      const std::string& key_name,
      const BoolDBusMethodCallback& callback) OVERRIDE;
  virtual void TpmAttestationGetCertificate(
      attestation::AttestationKeyType key_type,
      const std::string& key_name,
      const DataMethodCallback& callback) OVERRIDE;
  virtual void TpmAttestationGetPublicKey(
      attestation::AttestationKeyType key_type,
      const std::string& key_name,
      const DataMethodCallback& callback) OVERRIDE;
  virtual void TpmAttestationRegisterKey(
      attestation::AttestationKeyType key_type,
      const std::string& key_name,
      const AsyncMethodCallback& callback) OVERRIDE;
  virtual void TpmAttestationSignEnterpriseChallenge(
      attestation::AttestationKeyType key_type,
      const std::string& key_name,
      const std::string& domain,
      const std::string& device_id,
      attestation::AttestationChallengeOptions options,
      const std::string& challenge,
      const AsyncMethodCallback& callback) OVERRIDE;
  virtual void TpmAttestationSignSimpleChallenge(
      attestation::AttestationKeyType key_type,
      const std::string& key_name,
      const std::string& challenge,
      const AsyncMethodCallback& callback) OVERRIDE;
  virtual void TpmAttestationGetKeyPayload(
      attestation::AttestationKeyType key_type,
      const std::string& key_name,
      const DataMethodCallback& callback) OVERRIDE;
  virtual void TpmAttestationSetKeyPayload(
      attestation::AttestationKeyType key_type,
      const std::string& key_name,
      const std::string& payload,
      const BoolDBusMethodCallback& callback) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeCryptohomeClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_CRYPTOHOME_CLIENT_H_
