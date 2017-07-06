// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MOCK_CRYPTOHOME_CLIENT_H_
#define CHROMEOS_DBUS_MOCK_CRYPTOHOME_CLIENT_H_

#include <stdint.h>

#include <string>

#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockCryptohomeClient : public CryptohomeClient {
 public:
  MockCryptohomeClient();
  virtual ~MockCryptohomeClient();

  MOCK_METHOD1(Init, void(dbus::Bus* bus));
  MOCK_METHOD2(SetAsyncCallStatusHandlers,
               void(const AsyncCallStatusHandler& handler,
                    const AsyncCallStatusWithDataHandler& data_handler));
  MOCK_METHOD0(ResetAsyncCallStatusHandlers, void());
  MOCK_METHOD1(SetLowDiskSpaceHandler,
               void(const LowDiskSpaceHandler& handler));
  MOCK_METHOD1(WaitForServiceToBeAvailable,
               void(const WaitForServiceToBeAvailableCallback& callback));
  MOCK_METHOD1(IsMounted, void(const BoolDBusMethodCallback& callback));
  MOCK_METHOD1(Unmount, void(const BoolDBusMethodCallback& callback));
  MOCK_METHOD3(AsyncCheckKey,
               void(const cryptohome::Identification& cryptohome_id,
                    const std::string& key,
                    const AsyncMethodCallback& callback));
  MOCK_METHOD4(AsyncMigrateKey,
               void(const cryptohome::Identification& cryptohome_id,
                    const std::string& from_key,
                    const std::string& to_key,
                    const AsyncMethodCallback& callback));
  MOCK_METHOD2(AsyncRemove,
               void(const cryptohome::Identification& cryptohome_id,
                    const AsyncMethodCallback& callback));
  MOCK_METHOD3(RenameCryptohome,
               void(const cryptohome::Identification& id_from,
                    const cryptohome::Identification& id_to,
                    const ProtobufMethodCallback& callback));
  MOCK_METHOD2(GetAccountDiskUsage,
               void(const cryptohome::Identification& account_id,
                    const ProtobufMethodCallback& callback));

  MOCK_METHOD1(GetSystemSalt, void(const GetSystemSaltCallback& callback));
  MOCK_METHOD2(GetSanitizedUsername,
               void(const cryptohome::Identification& cryptohome_id,
                    const StringDBusMethodCallback& callback));
  MOCK_METHOD1(BlockingGetSanitizedUsername,
               std::string(const cryptohome::Identification& cryptohome_id));
  MOCK_METHOD4(AsyncMount,
               void(const cryptohome::Identification& cryptohome_id,
                    const std::string& key,
                    int flags,
                    const AsyncMethodCallback& callback));
  MOCK_METHOD4(AsyncAddKey,
               void(const cryptohome::Identification& cryptohome_id,
                    const std::string& key,
                    const std::string& new_key,
                    const AsyncMethodCallback& callback));
  MOCK_METHOD1(AsyncMountGuest,
               void(const AsyncMethodCallback& callback));
  MOCK_METHOD3(AsyncMountPublic,
               void(const cryptohome::Identification& public_mount_id,
                    int flags,
                    const AsyncMethodCallback& callback));
  MOCK_METHOD1(TpmIsReady, void(const BoolDBusMethodCallback& callback));
  MOCK_METHOD1(TpmIsEnabled, void(const BoolDBusMethodCallback& callback));
  MOCK_METHOD1(CallTpmIsEnabledAndBlock, bool(bool* enabled));
  MOCK_METHOD1(TpmGetPassword, void(const StringDBusMethodCallback& callback));
  MOCK_METHOD1(TpmIsOwned, void(const BoolDBusMethodCallback& callback));
  MOCK_METHOD1(CallTpmIsOwnedAndBlock, bool(bool* owned));
  MOCK_METHOD1(TpmIsBeingOwned, void(const BoolDBusMethodCallback& callback));
  MOCK_METHOD1(CallTpmIsBeingOwnedAndBlock, bool(bool* owning));
  void TpmCanAttemptOwnership(VoidDBusMethodCallback callback) override;
  void TpmClearStoredPassword(VoidDBusMethodCallback callback) override;
  MOCK_METHOD0(CallTpmClearStoredPasswordAndBlock, bool());
  MOCK_METHOD1(Pkcs11IsTpmTokenReady,
               void(const BoolDBusMethodCallback& callback));
  MOCK_METHOD1(Pkcs11GetTpmTokenInfo,
               void(const Pkcs11GetTpmTokenInfoCallback& callback));
  MOCK_METHOD2(Pkcs11GetTpmTokenInfoForUser,
               void(const cryptohome::Identification& cryptohome_id,
                    const Pkcs11GetTpmTokenInfoCallback& callback));
  MOCK_METHOD3(InstallAttributesGet,
               bool(const std::string& name,
                    std::vector<uint8_t>* value,
                    bool* successful));
  MOCK_METHOD3(InstallAttributesSet,
               bool(const std::string& name,
                    const std::vector<uint8_t>& value,
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
  MOCK_METHOD2(AsyncTpmAttestationCreateEnrollRequest,
               void(attestation::PrivacyCAType pca_type,
                    const AsyncMethodCallback& callback));
  MOCK_METHOD3(AsyncTpmAttestationEnroll,
               void(attestation::PrivacyCAType pca_type,
                    const std::string& pca_response,
                    const AsyncMethodCallback& callback));
  MOCK_METHOD5(
      AsyncTpmAttestationCreateCertRequest,
      void(attestation::PrivacyCAType pca_type,
           attestation::AttestationCertificateProfile certificate_profile,
           const cryptohome::Identification& cryptohome_id,
           const std::string& request_origin,
           const AsyncMethodCallback& callback));
  MOCK_METHOD5(AsyncTpmAttestationFinishCertRequest,
               void(const std::string& pca_response,
                    attestation::AttestationKeyType key_type,
                    const cryptohome::Identification& cryptohome_id,
                    const std::string& key_name,
                    const AsyncMethodCallback& callback));
  MOCK_METHOD4(TpmAttestationDoesKeyExist,
               void(attestation::AttestationKeyType key_type,
                    const cryptohome::Identification& cryptohome_id,
                    const std::string& key_name,
                    const BoolDBusMethodCallback& callback));
  MOCK_METHOD4(TpmAttestationGetCertificate,
               void(attestation::AttestationKeyType key_type,
                    const cryptohome::Identification& cryptohome_id,
                    const std::string& key_name,
                    const DataMethodCallback& callback));
  MOCK_METHOD4(TpmAttestationGetPublicKey,
               void(attestation::AttestationKeyType key_type,
                    const cryptohome::Identification& cryptohome_id,
                    const std::string& key_name,
                    const DataMethodCallback& callback));
  MOCK_METHOD4(TpmAttestationRegisterKey,
               void(attestation::AttestationKeyType key_type,
                    const cryptohome::Identification& cryptohome_id,
                    const std::string& key_name,
                    const AsyncMethodCallback& callback));
  MOCK_METHOD8(TpmAttestationSignEnterpriseChallenge,
               void(attestation::AttestationKeyType key_type,
                    const cryptohome::Identification& cryptohome_id,
                    const std::string& key_name,
                    const std::string& domain,
                    const std::string& device_id,
                    attestation::AttestationChallengeOptions options,
                    const std::string& challenge,
                    const AsyncMethodCallback& callback));
  MOCK_METHOD5(TpmAttestationSignSimpleChallenge,
               void(attestation::AttestationKeyType key_type,
                    const cryptohome::Identification& cryptohome_id,
                    const std::string& key_name,
                    const std::string& challenge,
                    const AsyncMethodCallback& callback));
  MOCK_METHOD4(TpmAttestationGetKeyPayload,
               void(attestation::AttestationKeyType key_type,
                    const cryptohome::Identification& cryptohome_id,
                    const std::string& key_name,
                    const DataMethodCallback& callback));
  MOCK_METHOD5(TpmAttestationSetKeyPayload,
               void(attestation::AttestationKeyType key_type,
                    const cryptohome::Identification& cryptohome_id,
                    const std::string& key_name,
                    const std::string& payload,
                    const BoolDBusMethodCallback& callback));
  MOCK_METHOD4(TpmAttestationDeleteKeys,
               void(attestation::AttestationKeyType key_type,
                    const cryptohome::Identification& cryptohome_id,
                    const std::string& key_prefix,
                    const BoolDBusMethodCallback& callback));
  MOCK_METHOD4(GetKeyDataEx,
               void(const cryptohome::Identification& cryptohome_id,
                    const cryptohome::AuthorizationRequest& auth,
                    const cryptohome::GetKeyDataRequest& request,
                    const ProtobufMethodCallback& callback));
  MOCK_METHOD4(CheckKeyEx,
               void(const cryptohome::Identification& cryptohome_id,
                    const cryptohome::AuthorizationRequest& auth,
                    const cryptohome::CheckKeyRequest& request,
                    const ProtobufMethodCallback& callback));
  MOCK_METHOD4(MountEx,
               void(const cryptohome::Identification& cryptohome_id,
                    const cryptohome::AuthorizationRequest& auth,
                    const cryptohome::MountRequest& request,
                    const ProtobufMethodCallback& callback));
  MOCK_METHOD4(AddKeyEx,
               void(const cryptohome::Identification& cryptohome_id,
                    const cryptohome::AuthorizationRequest& auth,
                    const cryptohome::AddKeyRequest& request,
                    const ProtobufMethodCallback& callback));
  MOCK_METHOD4(UpdateKeyEx,
               void(const cryptohome::Identification& cryptohome_id,
                    const cryptohome::AuthorizationRequest& auth,
                    const cryptohome::UpdateKeyRequest& request,
                    const ProtobufMethodCallback& callback));
  MOCK_METHOD4(RemoveKeyEx,
               void(const cryptohome::Identification& cryptohome_id,
                    const cryptohome::AuthorizationRequest& auth,
                    const cryptohome::RemoveKeyRequest& request,
                    const ProtobufMethodCallback& callback));
  MOCK_METHOD2(GetBootAttribute,
               void(const cryptohome::GetBootAttributeRequest& request,
                    const ProtobufMethodCallback& callback));
  MOCK_METHOD2(SetBootAttribute,
               void(const cryptohome::SetBootAttributeRequest& request,
                    const ProtobufMethodCallback& callback));
  MOCK_METHOD2(
      FlushAndSignBootAttributes,
      void(const cryptohome::FlushAndSignBootAttributesRequest& request,
           const ProtobufMethodCallback& callback));
  void MigrateToDircrypto(const cryptohome::Identification& cryptohome_id,
                          VoidDBusMethodCallback callback) override;
  MOCK_METHOD1(SetDircryptoMigrationProgressHandler,
               void(const DircryptoMigrationProgessHandler& handler));
  MOCK_METHOD2(
      RemoveFirmwareManagementParametersFromTpm,
      void(const cryptohome::RemoveFirmwareManagementParametersRequest& request,
           const ProtobufMethodCallback& callback));
  MOCK_METHOD2(
      SetFirmwareManagementParametersInTpm,
      void(const cryptohome::SetFirmwareManagementParametersRequest& request,
           const ProtobufMethodCallback& callback));
  MOCK_METHOD2(NeedsDircryptoMigration,
               void(const cryptohome::Identification& cryptohome_id,
                    const BoolDBusMethodCallback& callback));
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MOCK_CRYPTOHOME_CLIENT_H_
