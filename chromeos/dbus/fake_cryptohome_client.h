// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_CRYPTOHOME_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_CRYPTOHOME_CLIENT_H_

#include <stdint.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/timer/timer.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/cryptohome/key.pb.h"
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
  void SetLowDiskSpaceHandler(const LowDiskSpaceHandler& handler) override;
  void WaitForServiceToBeAvailable(
      WaitForServiceToBeAvailableCallback callback) override;
  void IsMounted(DBusMethodCallback<bool> callback) override;
  void Unmount(DBusMethodCallback<bool> callback) override;
  void AsyncCheckKey(const cryptohome::Identification& cryptohome_id,
                     const std::string& key,
                     AsyncMethodCallback callback) override;
  void AsyncMigrateKey(const cryptohome::Identification& cryptohome_id,
                       const std::string& from_key,
                       const std::string& to_key,
                       AsyncMethodCallback callback) override;
  void AsyncRemove(const cryptohome::Identification& cryptohome_id,
                   AsyncMethodCallback callback) override;
  void RenameCryptohome(
      const cryptohome::Identification& cryptohome_id_from,
      const cryptohome::Identification& cryptohome_id_to,
      DBusMethodCallback<cryptohome::BaseReply> callback) override;
  void GetAccountDiskUsage(
      const cryptohome::Identification& account_id,
      DBusMethodCallback<cryptohome::BaseReply> callback) override;
  void GetSystemSalt(
      DBusMethodCallback<std::vector<uint8_t>> callback) override;
  void GetSanitizedUsername(const cryptohome::Identification& cryptohome_id,
                            DBusMethodCallback<std::string> callback) override;
  std::string BlockingGetSanitizedUsername(
      const cryptohome::Identification& cryptohome_id) override;
  void AsyncMount(const cryptohome::Identification& cryptohome_id,
                  const std::string& key,
                  int flags,
                  AsyncMethodCallback callback) override;
  void AsyncAddKey(const cryptohome::Identification& cryptohome_id,
                   const std::string& key,
                   const std::string& new_key,
                   AsyncMethodCallback callback) override;
  void AsyncMountGuest(AsyncMethodCallback callback) override;
  void AsyncMountPublic(const cryptohome::Identification& public_mount_id,
                        int flags,
                        AsyncMethodCallback callback) override;
  void TpmIsReady(DBusMethodCallback<bool> callback) override;
  void TpmIsEnabled(DBusMethodCallback<bool> callback) override;
  bool CallTpmIsEnabledAndBlock(bool* enabled) override;
  void TpmGetPassword(DBusMethodCallback<std::string> callback) override;
  void TpmIsOwned(DBusMethodCallback<bool> callback) override;
  bool CallTpmIsOwnedAndBlock(bool* owned) override;
  void TpmIsBeingOwned(DBusMethodCallback<bool> callback) override;
  bool CallTpmIsBeingOwnedAndBlock(bool* owning) override;
  void TpmCanAttemptOwnership(VoidDBusMethodCallback callback) override;
  void TpmClearStoredPassword(VoidDBusMethodCallback callback) override;
  bool CallTpmClearStoredPasswordAndBlock() override;
  void Pkcs11IsTpmTokenReady(DBusMethodCallback<bool> callback) override;
  void Pkcs11GetTpmTokenInfo(
      DBusMethodCallback<TpmTokenInfo> callback) override;
  void Pkcs11GetTpmTokenInfoForUser(
      const cryptohome::Identification& cryptohome_id,
      DBusMethodCallback<TpmTokenInfo> callback) override;
  bool InstallAttributesGet(const std::string& name,
                            std::vector<uint8_t>* value,
                            bool* successful) override;
  bool InstallAttributesSet(const std::string& name,
                            const std::vector<uint8_t>& value,
                            bool* successful) override;
  bool InstallAttributesFinalize(bool* successful) override;
  void InstallAttributesIsReady(DBusMethodCallback<bool> callback) override;
  bool InstallAttributesIsInvalid(bool* is_invalid) override;
  bool InstallAttributesIsFirstInstall(bool* is_first_install) override;
  void TpmAttestationIsPrepared(DBusMethodCallback<bool> callback) override;
  void TpmAttestationIsEnrolled(DBusMethodCallback<bool> callback) override;
  void AsyncTpmAttestationCreateEnrollRequest(
      chromeos::attestation::PrivacyCAType pca_type,
      AsyncMethodCallback callback) override;
  void AsyncTpmAttestationEnroll(chromeos::attestation::PrivacyCAType pca_type,
                                 const std::string& pca_response,
                                 AsyncMethodCallback callback) override;
  void AsyncTpmAttestationCreateCertRequest(
      chromeos::attestation::PrivacyCAType pca_type,
      attestation::AttestationCertificateProfile certificate_profile,
      const cryptohome::Identification& cryptohome_id,
      const std::string& request_origin,
      AsyncMethodCallback callback) override;
  void AsyncTpmAttestationFinishCertRequest(
      const std::string& pca_response,
      attestation::AttestationKeyType key_type,
      const cryptohome::Identification& cryptohome_id,
      const std::string& key_name,
      AsyncMethodCallback callback) override;
  void TpmAttestationDoesKeyExist(
      attestation::AttestationKeyType key_type,
      const cryptohome::Identification& cryptohome_id,
      const std::string& key_name,
      DBusMethodCallback<bool> callback) override;
  void TpmAttestationGetCertificate(
      attestation::AttestationKeyType key_type,
      const cryptohome::Identification& cryptohome_id,
      const std::string& key_name,
      DBusMethodCallback<TpmAttestationDataResult> callback) override;
  void TpmAttestationGetPublicKey(
      attestation::AttestationKeyType key_type,
      const cryptohome::Identification& cryptohome_id,
      const std::string& key_name,
      DBusMethodCallback<TpmAttestationDataResult> callback) override;
  void TpmAttestationRegisterKey(
      attestation::AttestationKeyType key_type,
      const cryptohome::Identification& cryptohome_id,
      const std::string& key_name,
      AsyncMethodCallback callback) override;
  void TpmAttestationSignEnterpriseChallenge(
      attestation::AttestationKeyType key_type,
      const cryptohome::Identification& cryptohome_id,
      const std::string& key_name,
      const std::string& domain,
      const std::string& device_id,
      attestation::AttestationChallengeOptions options,
      const std::string& challenge,
      AsyncMethodCallback callback) override;
  void TpmAttestationSignSimpleChallenge(
      attestation::AttestationKeyType key_type,
      const cryptohome::Identification& cryptohome_id,
      const std::string& key_name,
      const std::string& challenge,
      AsyncMethodCallback callback) override;
  void TpmAttestationGetKeyPayload(
      attestation::AttestationKeyType key_type,
      const cryptohome::Identification& cryptohome_id,
      const std::string& key_name,
      DBusMethodCallback<TpmAttestationDataResult> callback) override;
  void TpmAttestationSetKeyPayload(
      attestation::AttestationKeyType key_type,
      const cryptohome::Identification& cryptohome_id,
      const std::string& key_name,
      const std::string& payload,
      DBusMethodCallback<bool> callback) override;
  void TpmAttestationDeleteKeys(attestation::AttestationKeyType key_type,
                                const cryptohome::Identification& cryptohome_id,
                                const std::string& key_prefix,
                                DBusMethodCallback<bool> callback) override;
  void TpmGetVersion(DBusMethodCallback<TpmVersionInfo> callback) override;
  void GetKeyDataEx(
      const cryptohome::Identification& cryptohome_id,
      const cryptohome::AuthorizationRequest& auth,
      const cryptohome::GetKeyDataRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) override;
  void CheckKeyEx(const cryptohome::Identification& cryptohome_id,
                  const cryptohome::AuthorizationRequest& auth,
                  const cryptohome::CheckKeyRequest& request,
                  DBusMethodCallback<cryptohome::BaseReply> callback) override;
  void MountEx(const cryptohome::Identification& cryptohome_id,
               const cryptohome::AuthorizationRequest& auth,
               const cryptohome::MountRequest& request,
               DBusMethodCallback<cryptohome::BaseReply> callback) override;
  void AddKeyEx(const cryptohome::Identification& cryptohome_id,
                const cryptohome::AuthorizationRequest& auth,
                const cryptohome::AddKeyRequest& request,
                DBusMethodCallback<cryptohome::BaseReply> callback) override;
  void UpdateKeyEx(const cryptohome::Identification& cryptohome_id,
                   const cryptohome::AuthorizationRequest& auth,
                   const cryptohome::UpdateKeyRequest& request,
                   DBusMethodCallback<cryptohome::BaseReply> callback) override;
  void RemoveKeyEx(const cryptohome::Identification& cryptohome_id,
                   const cryptohome::AuthorizationRequest& auth,
                   const cryptohome::RemoveKeyRequest& request,
                   DBusMethodCallback<cryptohome::BaseReply> callback) override;
  void GetBootAttribute(
      const cryptohome::GetBootAttributeRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) override;
  void SetBootAttribute(
      const cryptohome::SetBootAttributeRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) override;
  void FlushAndSignBootAttributes(
      const cryptohome::FlushAndSignBootAttributesRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) override;
  void MigrateToDircrypto(const cryptohome::Identification& cryptohome_id,
                          const cryptohome::MigrateToDircryptoRequest& request,
                          VoidDBusMethodCallback callback) override;
  void SetDircryptoMigrationProgressHandler(
      const DircryptoMigrationProgessHandler& handler) override;
  void RemoveFirmwareManagementParametersFromTpm(
      const cryptohome::RemoveFirmwareManagementParametersRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) override;
  void SetFirmwareManagementParametersInTpm(
      const cryptohome::SetFirmwareManagementParametersRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) override;
  void NeedsDircryptoMigration(const cryptohome::Identification& cryptohome_id,
                               DBusMethodCallback<bool> callback) override;

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
  void set_system_salt(const std::vector<uint8_t>& system_salt) {
    system_salt_ = system_salt;
  }

  // Returns the stub system salt as raw bytes. (not as a string encoded in the
  // format used by SystemSaltGetter::ConvertRawSaltToHexString()).
  static std::vector<uint8_t> GetStubSystemSalt();

  // Sets the needs dircrypto migration value.
  void set_needs_dircrypto_migration(bool needs_migration) {
    needs_dircrypto_migration_ = needs_migration;
  }

  void set_tpm_attestation_is_enrolled(bool enrolled) {
    tpm_attestation_is_enrolled_ = enrolled;
  }

  void set_tpm_attestation_is_prepared(bool prepared) {
    tpm_attestation_is_prepared_ = prepared;
  }

  void set_tpm_attestation_does_key_exist_should_succeed(bool should_succeed) {
    tpm_attestation_does_key_exist_should_succeed_ = should_succeed;
  }

  void SetTpmAttestationUserCertificate(
      const cryptohome::Identification& cryptohome_id,
      const std::string& key_name,
      const std::string& certificate);

  void SetTpmAttestationDeviceCertificate(const std::string& key_name,
                                          const std::string& certificate);

  base::Optional<std::string> GetTpmAttestationDeviceKeyPayload(
      const std::string& key_name) const;

  void SetTpmAttestationDeviceKeyPayload(const std::string& key_name,
                                         const std::string& payload);

  DircryptoMigrationProgessHandler dircrypto_migration_progress_handler() {
    return dircrypto_migration_progress_handler_;
  }

 private:
  void ReturnProtobufMethodCallback(
      const cryptohome::BaseReply& reply,
      DBusMethodCallback<cryptohome::BaseReply> callback);

  // Posts tasks which return fake results to the UI thread.
  void ReturnAsyncMethodResult(AsyncMethodCallback callback);

  // Posts tasks which return fake data to the UI thread.
  void ReturnAsyncMethodData(AsyncMethodCallback callback,
                             const std::string& data);

  // This method is used to implement ReturnAsyncMethodResult without data.
  void ReturnAsyncMethodResultInternal(AsyncMethodCallback callback);

  // This method is used to implement ReturnAsyncMethodResult with data.
  void ReturnAsyncMethodDataInternal(AsyncMethodCallback callback,
                                     const std::string& data);

  // This method is used to implement MigrateToDircrypto with simulated progress
  // updates.
  void OnDircryptoMigrationProgressUpdated();

  bool service_is_available_;
  int async_call_id_;
  AsyncCallStatusHandler async_call_status_handler_;
  AsyncCallStatusWithDataHandler async_call_status_data_handler_;
  bool unmount_result_;
  std::vector<uint8_t> system_salt_;

  std::vector<WaitForServiceToBeAvailableCallback>
      pending_wait_for_service_to_be_available_callbacks_;

  // A stub store for InstallAttributes, mapping an attribute name to the
  // associated data blob. Used to implement InstallAttributesSet and -Get.
  std::map<std::string, std::vector<uint8_t>> install_attrs_;
  bool locked_;

  std::map<cryptohome::Identification, cryptohome::KeyData> key_data_map_;

  // User attestation certificate mapped by cryptohome_id and key_name.
  std::map<std::pair<cryptohome::Identification, std::string>, std::string>
      user_certificate_map_;

  // Device attestation certificate mapped by key_name.
  std::map<std::string, std::string> device_certificate_map_;

  // Device key payload data mapped by key_name.
  std::map<std::string, std::string> device_key_payload_map_;

  DircryptoMigrationProgessHandler dircrypto_migration_progress_handler_;
  base::RepeatingTimer dircrypto_migration_progress_timer_;
  uint64_t dircrypto_migration_progress_;

  bool needs_dircrypto_migration_ = false;
  bool tpm_attestation_is_enrolled_ = true;
  bool tpm_attestation_is_prepared_ = true;
  bool tpm_attestation_does_key_exist_should_succeed_ = true;

  base::WeakPtrFactory<FakeCryptohomeClient> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeCryptohomeClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_CRYPTOHOME_CLIENT_H_
