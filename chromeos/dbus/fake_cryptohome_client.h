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
  virtual ~FakeCryptohomeClient();

  virtual void Init(dbus::Bus* bus) OVERRIDE;
  virtual void SetAsyncCallStatusHandlers(
      const AsyncCallStatusHandler& handler,
      const AsyncCallStatusWithDataHandler& data_handler) OVERRIDE;
  virtual void ResetAsyncCallStatusHandlers() OVERRIDE;
  virtual void WaitForServiceToBeAvailable(
      const WaitForServiceToBeAvailableCallback& callback) OVERRIDE;
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
  virtual void GetSystemSalt(const GetSystemSaltCallback& callback) OVERRIDE;
  virtual void GetSanitizedUsername(
      const std::string& username,
      const StringDBusMethodCallback& callback) OVERRIDE;
  virtual std::string BlockingGetSanitizedUsername(
      const std::string& username) OVERRIDE;
  virtual void AsyncMount(const std::string& username,
                          const std::string& key,
                          int flags,
                          const AsyncMethodCallback& callback) OVERRIDE;
  virtual void AsyncAddKey(const std::string& username,
                           const std::string& key,
                           const std::string& new_key,
                           const AsyncMethodCallback& callback) OVERRIDE;
  virtual void AsyncMountGuest(const AsyncMethodCallback& callback) OVERRIDE;
  virtual void AsyncMountPublic(const std::string& public_mount_id,
                                int flags,
                                const AsyncMethodCallback& callback) OVERRIDE;
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
  virtual void Pkcs11GetTpmTokenInfoForUser(
      const std::string& username,
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
      chromeos::attestation::PrivacyCAType pca_type,
      const AsyncMethodCallback& callback) OVERRIDE;
  virtual void AsyncTpmAttestationEnroll(
      chromeos::attestation::PrivacyCAType pca_type,
      const std::string& pca_response,
      const AsyncMethodCallback& callback) OVERRIDE;
  virtual void AsyncTpmAttestationCreateCertRequest(
      chromeos::attestation::PrivacyCAType pca_type,
      attestation::AttestationCertificateProfile certificate_profile,
      const std::string& user_id,
      const std::string& request_origin,
      const AsyncMethodCallback& callback) OVERRIDE;
  virtual void AsyncTpmAttestationFinishCertRequest(
      const std::string& pca_response,
      attestation::AttestationKeyType key_type,
      const std::string& user_id,
      const std::string& key_name,
      const AsyncMethodCallback& callback) OVERRIDE;
  virtual void TpmAttestationDoesKeyExist(
      attestation::AttestationKeyType key_type,
      const std::string& user_id,
      const std::string& key_name,
      const BoolDBusMethodCallback& callback) OVERRIDE;
  virtual void TpmAttestationGetCertificate(
      attestation::AttestationKeyType key_type,
      const std::string& user_id,
      const std::string& key_name,
      const DataMethodCallback& callback) OVERRIDE;
  virtual void TpmAttestationGetPublicKey(
      attestation::AttestationKeyType key_type,
      const std::string& user_id,
      const std::string& key_name,
      const DataMethodCallback& callback) OVERRIDE;
  virtual void TpmAttestationRegisterKey(
      attestation::AttestationKeyType key_type,
      const std::string& user_id,
      const std::string& key_name,
      const AsyncMethodCallback& callback) OVERRIDE;
  virtual void TpmAttestationSignEnterpriseChallenge(
      attestation::AttestationKeyType key_type,
      const std::string& user_id,
      const std::string& key_name,
      const std::string& domain,
      const std::string& device_id,
      attestation::AttestationChallengeOptions options,
      const std::string& challenge,
      const AsyncMethodCallback& callback) OVERRIDE;
  virtual void TpmAttestationSignSimpleChallenge(
      attestation::AttestationKeyType key_type,
      const std::string& user_id,
      const std::string& key_name,
      const std::string& challenge,
      const AsyncMethodCallback& callback) OVERRIDE;
  virtual void TpmAttestationGetKeyPayload(
      attestation::AttestationKeyType key_type,
      const std::string& user_id,
      const std::string& key_name,
      const DataMethodCallback& callback) OVERRIDE;
  virtual void TpmAttestationSetKeyPayload(
      attestation::AttestationKeyType key_type,
      const std::string& user_id,
      const std::string& key_name,
      const std::string& payload,
      const BoolDBusMethodCallback& callback) OVERRIDE;
  virtual void TpmAttestationDeleteKeys(
      attestation::AttestationKeyType key_type,
      const std::string& user_id,
      const std::string& key_prefix,
      const BoolDBusMethodCallback& callback) OVERRIDE;
  virtual void GetKeyDataEx(
      const cryptohome::AccountIdentifier& id,
      const cryptohome::AuthorizationRequest& auth,
      const cryptohome::GetKeyDataRequest& request,
      const ProtobufMethodCallback& callback) OVERRIDE;
  virtual void CheckKeyEx(
      const cryptohome::AccountIdentifier& id,
      const cryptohome::AuthorizationRequest& auth,
      const cryptohome::CheckKeyRequest& request,
      const ProtobufMethodCallback& callback) OVERRIDE;
  virtual void MountEx(
      const cryptohome::AccountIdentifier& id,
      const cryptohome::AuthorizationRequest& auth,
      const cryptohome::MountRequest& request,
      const ProtobufMethodCallback& callback) OVERRIDE;
  virtual void AddKeyEx(
      const cryptohome::AccountIdentifier& id,
      const cryptohome::AuthorizationRequest& auth,
      const cryptohome::AddKeyRequest& request,
      const ProtobufMethodCallback& callback) OVERRIDE;
  virtual void UpdateKeyEx(
      const cryptohome::AccountIdentifier& id,
      const cryptohome::AuthorizationRequest& auth,
      const cryptohome::UpdateKeyRequest& request,
      const ProtobufMethodCallback& callback) OVERRIDE;
  virtual void RemoveKeyEx(const cryptohome::AccountIdentifier& id,
                           const cryptohome::AuthorizationRequest& auth,
                           const cryptohome::RemoveKeyRequest& request,
                           const ProtobufMethodCallback& callback) OVERRIDE;
  virtual void GetBootAttribute(
      const cryptohome::GetBootAttributeRequest& request,
      const ProtobufMethodCallback& callback) OVERRIDE;
  virtual void SetBootAttribute(
      const cryptohome::SetBootAttributeRequest& request,
      const ProtobufMethodCallback& callback) OVERRIDE;
  virtual void FlushAndSignBootAttributes(
      const cryptohome::FlushAndSignBootAttributesRequest& request,
      const ProtobufMethodCallback& callback) OVERRIDE;

  // Changes the behavior of WaitForServiceToBeAvailable(). This method runs
  // pending callbacks if is_available is true.
  void SetServiceIsAvailable(bool is_available);

  // Sets the unmount result of Unmount() call.
  void set_unmount_result(bool result) {
    unmount_result_= result;
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
  int tpm_is_ready_counter_;
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
