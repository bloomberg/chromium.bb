// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_CRYPTOHOME_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_CRYPTOHOME_CLIENT_H_

#include <map>

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/cryptohome_client.h"

namespace chromeos {

class CHROMEOS_EXPORT FakeCryptohomeClient : public CryptohomeClient {
 public:
  FakeCryptohomeClient();
  ~FakeCryptohomeClient() override;

  void Init(dbus::Bus* bus) override;
  void SetAsyncCallStatusHandlers(
      const AsyncCallStatusHandler& handler,
      const AsyncCallStatusWithDataHandler& data_handler) override;
  void ResetAsyncCallStatusHandlers() override;
  void WaitForServiceToBeAvailable(
      const WaitForServiceToBeAvailableCallback& callback) override;
  void IsMounted(const BoolDBusMethodCallback& callback) override;
  bool Unmount(bool* success) override;
  void AsyncCheckKey(const std::string& username,
                     const std::string& key,
                     const AsyncMethodCallback& callback) override;
  void AsyncMigrateKey(const std::string& username,
                       const std::string& from_key,
                       const std::string& to_key,
                       const AsyncMethodCallback& callback) override;
  void AsyncRemove(const std::string& username,
                   const AsyncMethodCallback& callback) override;
  void GetSystemSalt(const GetSystemSaltCallback& callback) override;
  void GetSanitizedUsername(const std::string& username,
                            const StringDBusMethodCallback& callback) override;
  std::string BlockingGetSanitizedUsername(
      const std::string& username) override;
  void AsyncMount(const std::string& username,
                  const std::string& key,
                  int flags,
                  const AsyncMethodCallback& callback) override;
  void AsyncAddKey(const std::string& username,
                   const std::string& key,
                   const std::string& new_key,
                   const AsyncMethodCallback& callback) override;
  void AsyncMountGuest(const AsyncMethodCallback& callback) override;
  void AsyncMountPublic(const std::string& public_mount_id,
                        int flags,
                        const AsyncMethodCallback& callback) override;
  void TpmIsReady(const BoolDBusMethodCallback& callback) override;
  void TpmIsEnabled(const BoolDBusMethodCallback& callback) override;
  bool CallTpmIsEnabledAndBlock(bool* enabled) override;
  void TpmGetPassword(const StringDBusMethodCallback& callback) override;
  void TpmIsOwned(const BoolDBusMethodCallback& callback) override;
  bool CallTpmIsOwnedAndBlock(bool* owned) override;
  void TpmIsBeingOwned(const BoolDBusMethodCallback& callback) override;
  bool CallTpmIsBeingOwnedAndBlock(bool* owning) override;
  void TpmCanAttemptOwnership(const VoidDBusMethodCallback& callback) override;
  void TpmClearStoredPassword(const VoidDBusMethodCallback& callback) override;
  bool CallTpmClearStoredPasswordAndBlock() override;
  void Pkcs11IsTpmTokenReady(const BoolDBusMethodCallback& callback) override;
  void Pkcs11GetTpmTokenInfo(
      const Pkcs11GetTpmTokenInfoCallback& callback) override;
  void Pkcs11GetTpmTokenInfoForUser(
      const std::string& username,
      const Pkcs11GetTpmTokenInfoCallback& callback) override;
  bool InstallAttributesGet(const std::string& name,
                            std::vector<uint8>* value,
                            bool* successful) override;
  bool InstallAttributesSet(const std::string& name,
                            const std::vector<uint8>& value,
                            bool* successful) override;
  bool InstallAttributesFinalize(bool* successful) override;
  void InstallAttributesIsReady(
      const BoolDBusMethodCallback& callback) override;
  bool InstallAttributesIsInvalid(bool* is_invalid) override;
  bool InstallAttributesIsFirstInstall(bool* is_first_install) override;
  void TpmAttestationIsPrepared(
      const BoolDBusMethodCallback& callback) override;
  void TpmAttestationIsEnrolled(
      const BoolDBusMethodCallback& callback) override;
  void AsyncTpmAttestationCreateEnrollRequest(
      chromeos::attestation::PrivacyCAType pca_type,
      const AsyncMethodCallback& callback) override;
  void AsyncTpmAttestationEnroll(chromeos::attestation::PrivacyCAType pca_type,
                                 const std::string& pca_response,
                                 const AsyncMethodCallback& callback) override;
  void AsyncTpmAttestationCreateCertRequest(
      chromeos::attestation::PrivacyCAType pca_type,
      attestation::AttestationCertificateProfile certificate_profile,
      const std::string& user_id,
      const std::string& request_origin,
      const AsyncMethodCallback& callback) override;
  void AsyncTpmAttestationFinishCertRequest(
      const std::string& pca_response,
      attestation::AttestationKeyType key_type,
      const std::string& user_id,
      const std::string& key_name,
      const AsyncMethodCallback& callback) override;
  void TpmAttestationDoesKeyExist(
      attestation::AttestationKeyType key_type,
      const std::string& user_id,
      const std::string& key_name,
      const BoolDBusMethodCallback& callback) override;
  void TpmAttestationGetCertificate(
      attestation::AttestationKeyType key_type,
      const std::string& user_id,
      const std::string& key_name,
      const DataMethodCallback& callback) override;
  void TpmAttestationGetPublicKey(attestation::AttestationKeyType key_type,
                                  const std::string& user_id,
                                  const std::string& key_name,
                                  const DataMethodCallback& callback) override;
  void TpmAttestationRegisterKey(attestation::AttestationKeyType key_type,
                                 const std::string& user_id,
                                 const std::string& key_name,
                                 const AsyncMethodCallback& callback) override;
  void TpmAttestationSignEnterpriseChallenge(
      attestation::AttestationKeyType key_type,
      const std::string& user_id,
      const std::string& key_name,
      const std::string& domain,
      const std::string& device_id,
      attestation::AttestationChallengeOptions options,
      const std::string& challenge,
      const AsyncMethodCallback& callback) override;
  void TpmAttestationSignSimpleChallenge(
      attestation::AttestationKeyType key_type,
      const std::string& user_id,
      const std::string& key_name,
      const std::string& challenge,
      const AsyncMethodCallback& callback) override;
  void TpmAttestationGetKeyPayload(attestation::AttestationKeyType key_type,
                                   const std::string& user_id,
                                   const std::string& key_name,
                                   const DataMethodCallback& callback) override;
  void TpmAttestationSetKeyPayload(
      attestation::AttestationKeyType key_type,
      const std::string& user_id,
      const std::string& key_name,
      const std::string& payload,
      const BoolDBusMethodCallback& callback) override;
  void TpmAttestationDeleteKeys(
      attestation::AttestationKeyType key_type,
      const std::string& user_id,
      const std::string& key_prefix,
      const BoolDBusMethodCallback& callback) override;
  void GetKeyDataEx(const cryptohome::AccountIdentifier& id,
                    const cryptohome::AuthorizationRequest& auth,
                    const cryptohome::GetKeyDataRequest& request,
                    const ProtobufMethodCallback& callback) override;
  void CheckKeyEx(const cryptohome::AccountIdentifier& id,
                  const cryptohome::AuthorizationRequest& auth,
                  const cryptohome::CheckKeyRequest& request,
                  const ProtobufMethodCallback& callback) override;
  void MountEx(const cryptohome::AccountIdentifier& id,
               const cryptohome::AuthorizationRequest& auth,
               const cryptohome::MountRequest& request,
               const ProtobufMethodCallback& callback) override;
  void AddKeyEx(const cryptohome::AccountIdentifier& id,
                const cryptohome::AuthorizationRequest& auth,
                const cryptohome::AddKeyRequest& request,
                const ProtobufMethodCallback& callback) override;
  void UpdateKeyEx(const cryptohome::AccountIdentifier& id,
                   const cryptohome::AuthorizationRequest& auth,
                   const cryptohome::UpdateKeyRequest& request,
                   const ProtobufMethodCallback& callback) override;
  void RemoveKeyEx(const cryptohome::AccountIdentifier& id,
                   const cryptohome::AuthorizationRequest& auth,
                   const cryptohome::RemoveKeyRequest& request,
                   const ProtobufMethodCallback& callback) override;
  void GetBootAttribute(const cryptohome::GetBootAttributeRequest& request,
                        const ProtobufMethodCallback& callback) override;
  void SetBootAttribute(const cryptohome::SetBootAttributeRequest& request,
                        const ProtobufMethodCallback& callback) override;
  void FlushAndSignBootAttributes(
      const cryptohome::FlushAndSignBootAttributesRequest& request,
      const ProtobufMethodCallback& callback) override;

  // Changes the behavior of WaitForServiceToBeAvailable(). This method runs
  // pending callbacks if is_available is true.
  void SetServiceIsAvailable(bool is_available);

  // Sets the unmount result of Unmount() call.
  void set_unmount_result(bool result) {
    unmount_result_ = result;
  }

  // Sets the system salt which will be returned from GetSystemSalt(). By
  // default, GetSystemSalt() returns the value generated by
  // GetStubSystemSalt().
  void set_system_salt(const std::vector<uint8>& system_salt) {
    system_salt_ = system_salt;
  }

  // Returns the stub system salt as raw bytes. (not as a string encoded in the
  // format used by SystemSaltGetter::ConvertRawSaltToHexString()).
  static std::vector<uint8> GetStubSystemSalt();

 private:
  void ReturnProtobufMethodCallback(
      const cryptohome::BaseReply& reply,
      const ProtobufMethodCallback& callback);

  // Posts tasks which return fake results to the UI thread.
  void ReturnAsyncMethodResult(const AsyncMethodCallback& callback,
                               bool returns_data);

  // This method is used to implement ReturnAsyncMethodResult.
  void ReturnAsyncMethodResultInternal(const AsyncMethodCallback& callback,
                                       bool returns_data);

  bool service_is_available_;
  int async_call_id_;
  AsyncCallStatusHandler async_call_status_handler_;
  AsyncCallStatusWithDataHandler async_call_status_data_handler_;
  bool unmount_result_;
  std::vector<uint8> system_salt_;

  std::vector<WaitForServiceToBeAvailableCallback>
      pending_wait_for_service_to_be_available_callbacks_;

  // A stub store for InstallAttributes, mapping an attribute name to the
  // associated data blob. Used to implement InstallAttributesSet and -Get.
  std::map<std::string, std::vector<uint8> > install_attrs_;
  bool locked_;
  base::WeakPtrFactory<FakeCryptohomeClient> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeCryptohomeClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_CRYPTOHOME_CLIENT_H_
