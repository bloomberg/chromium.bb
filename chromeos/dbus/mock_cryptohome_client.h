// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MOCK_CRYPTOHOME_CLIENT_H_
#define CHROMEOS_DBUS_MOCK_CRYPTOHOME_CLIENT_H_

#include <string>

#include "chromeos/dbus/cryptohome_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockCryptohomeClient : public CryptohomeClient {
 public:
  MockCryptohomeClient();
  virtual ~MockCryptohomeClient();

  MOCK_METHOD2(SetAsyncCallStatusHandlers,
               void(const AsyncCallStatusHandler& handler,
                    const AsyncCallStatusWithDataHandler& data_handler));
  MOCK_METHOD0(ResetAsyncCallStatusHandlers, void());
  MOCK_METHOD1(IsMounted, void(const BoolDBusMethodCallback& callback));
  MOCK_METHOD1(Unmount, bool(bool* success));
  MOCK_METHOD3(AsyncCheckKey,
               void(const std::string& username,
                    const std::string& key,
                    const AsyncMethodCallback& callback));
  MOCK_METHOD4(AsyncMigrateKey,
               void(const std::string& username,
                    const std::string& from_key,
                    const std::string& to_key,
                    const AsyncMethodCallback& callback));
  MOCK_METHOD2(AsyncRemove, void(const std::string& username,
                                 const AsyncMethodCallback& callback));
  MOCK_METHOD1(GetSystemSalt, bool(std::vector<uint8>* salt));
  MOCK_METHOD2(GetSanitizedUsername,
               void(const std::string& username,
                    const StringDBusMethodCallback& callback));
  MOCK_METHOD4(AsyncMount, void(const std::string& username,
                                const std::string& key,
                                int flags,
                                const AsyncMethodCallback& callback));
  MOCK_METHOD1(AsyncMountGuest,
               void(const AsyncMethodCallback& callback));
  MOCK_METHOD1(TpmIsReady, void(const BoolDBusMethodCallback& callback));
  MOCK_METHOD1(TpmIsEnabled, void(const BoolDBusMethodCallback& callback));
  MOCK_METHOD1(CallTpmIsEnabledAndBlock, bool(bool* enabled));
  MOCK_METHOD1(TpmGetPassword, void(const StringDBusMethodCallback& callback));
  MOCK_METHOD1(TpmIsOwned, void(const BoolDBusMethodCallback& callback));
  MOCK_METHOD1(CallTpmIsOwnedAndBlock, bool(bool* owned));
  MOCK_METHOD1(TpmIsBeingOwned, void(const BoolDBusMethodCallback& callback));
  MOCK_METHOD1(CallTpmIsBeingOwnedAndBlock, bool(bool* owning));
  MOCK_METHOD1(TpmCanAttemptOwnership,
               void(const VoidDBusMethodCallback& callback));
  MOCK_METHOD1(TpmClearStoredPassword,
               void(const VoidDBusMethodCallback& callback));
  MOCK_METHOD0(CallTpmClearStoredPasswordAndBlock, bool());
  MOCK_METHOD1(Pkcs11IsTpmTokenReady,
               void(const BoolDBusMethodCallback& callback));
  MOCK_METHOD1(Pkcs11GetTpmTokenInfo,
               void(const Pkcs11GetTpmTokenInfoCallback& callback));
  MOCK_METHOD3(InstallAttributesGet,
               bool(const std::string& name,
                    std::vector<uint8>* value,
                    bool* successful));
  MOCK_METHOD3(InstallAttributesSet,
               bool(const std::string& name,
                    const std::vector<uint8>& value,
                    bool* successful));
  MOCK_METHOD1(InstallAttributesFinalize, bool(bool* successful));
  MOCK_METHOD1(InstallAttributesIsReady,
               void(const BoolDBusMethodCallback& callback));
  MOCK_METHOD1(InstallAttributesIsInvalid, bool(bool* is_invalid));
  MOCK_METHOD1(InstallAttributesIsFirstInstall, bool(bool* is_first_install));
  MOCK_METHOD1(TpmAttestationIsPrepared,
               void(const BoolDBusMethodCallback& callback));
  MOCK_METHOD1(TpmAttestationIsEnrolled,
               void(const BoolDBusMethodCallback& callback));
  MOCK_METHOD1(AsyncTpmAttestationCreateEnrollRequest,
               void(const AsyncMethodCallback& callback));
  MOCK_METHOD2(AsyncTpmAttestationEnroll,
               void(const std::string& pca_response,
                    const AsyncMethodCallback& callback));
  MOCK_METHOD2(AsyncTpmAttestationCreateCertRequest,
               void(int options,
                    const AsyncMethodCallback& callback));
  MOCK_METHOD4(AsyncTpmAttestationFinishCertRequest,
               void(const std::string& pca_response,
                    AttestationKeyType key_type,
                    const std::string& key_name,
                    const AsyncMethodCallback& callback));
  MOCK_METHOD3(TpmAttestationDoesKeyExist,
               void(AttestationKeyType key_type,
                    const std::string& key_name,
                    const BoolDBusMethodCallback& callback));
  MOCK_METHOD3(TpmAttestationGetCertificate,
               void(AttestationKeyType key_type,
                    const std::string& key_name,
                    const DataMethodCallback& callback));
  MOCK_METHOD3(TpmAttestationGetPublicKey,
               void(AttestationKeyType key_type,
                    const std::string& key_name,
                    const DataMethodCallback& callback));
  MOCK_METHOD3(TpmAttestationRegisterKey,
               void(AttestationKeyType key_type,
                    const std::string& key_name,
                    const AsyncMethodCallback& callback));
  MOCK_METHOD6(TpmAttestationSignEnterpriseChallenge,
               void(AttestationKeyType key_type,
                    const std::string& key_name,
                    const std::string& domain,
                    const std::string& device_id,
                    const std::string& challenge,
                    const AsyncMethodCallback& callback));
  MOCK_METHOD4(TpmAttestationSignSimpleChallenge,
               void(AttestationKeyType key_type,
                    const std::string& key_name,
                    const std::string& challenge,
                    const AsyncMethodCallback& callback));
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MOCK_CRYPTOHOME_CLIENT_H_
