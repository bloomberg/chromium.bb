// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_cryptohome_client.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "chromeos/chromeos_paths.h"
#include "chromeos/dbus/cryptohome/key.pb.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "third_party/protobuf/src/google/protobuf/io/coded_stream.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"

namespace chromeos {

FakeCryptohomeClient::FakeCryptohomeClient()
    : service_is_available_(true),
      async_call_id_(1),
      tpm_is_ready_counter_(0),
      unmount_result_(true),
      system_salt_(GetStubSystemSalt()),
      weak_ptr_factory_(this) {
  base::FilePath cache_path;
  locked_ = PathService::Get(chromeos::FILE_INSTALL_ATTRIBUTES, &cache_path) &&
            base::PathExists(cache_path);
}

FakeCryptohomeClient::~FakeCryptohomeClient() {}

void FakeCryptohomeClient::Init(dbus::Bus* bus) {
}

void FakeCryptohomeClient::SetAsyncCallStatusHandlers(
    const AsyncCallStatusHandler& handler,
    const AsyncCallStatusWithDataHandler& data_handler) {
  async_call_status_handler_ = handler;
  async_call_status_data_handler_ = data_handler;
}

void FakeCryptohomeClient::ResetAsyncCallStatusHandlers() {
  async_call_status_handler_.Reset();
  async_call_status_data_handler_.Reset();
}

void FakeCryptohomeClient::WaitForServiceToBeAvailable(
    const WaitForServiceToBeAvailableCallback& callback) {
  if (service_is_available_) {
    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::Bind(callback, true));
  } else {
    pending_wait_for_service_to_be_available_callbacks_.push_back(callback);
  }
}

void FakeCryptohomeClient::IsMounted(
    const BoolDBusMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, true));
}

bool FakeCryptohomeClient::Unmount(bool* success) {
  *success = unmount_result_;
  return true;
}

void FakeCryptohomeClient::AsyncCheckKey(
    const std::string& username,
    const std::string& key,
    const AsyncMethodCallback& callback) {
  ReturnAsyncMethodResult(callback, false);
}

void FakeCryptohomeClient::AsyncMigrateKey(
    const std::string& username,
    const std::string& from_key,
    const std::string& to_key,
    const AsyncMethodCallback& callback) {
  ReturnAsyncMethodResult(callback, false);
}

void FakeCryptohomeClient::AsyncRemove(
    const std::string& username,
    const AsyncMethodCallback& callback) {
  ReturnAsyncMethodResult(callback, false);
}

void FakeCryptohomeClient::GetSystemSalt(
    const GetSystemSaltCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, system_salt_));
}

void FakeCryptohomeClient::GetSanitizedUsername(
    const std::string& username,
    const StringDBusMethodCallback& callback) {
  // Even for stub implementation we have to return different values so that
  // multi-profiles would work.
  std::string sanitized_username = GetStubSanitizedUsername(username);
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, sanitized_username));
}

std::string FakeCryptohomeClient::BlockingGetSanitizedUsername(
    const std::string& username) {
  return GetStubSanitizedUsername(username);
}

void FakeCryptohomeClient::AsyncMount(const std::string& username,
                                          const std::string& key,
                                          int flags,
                                          const AsyncMethodCallback& callback) {
  ReturnAsyncMethodResult(callback, false);
}

void FakeCryptohomeClient::AsyncAddKey(
    const std::string& username,
    const std::string& key,
    const std::string& new_key,
    const AsyncMethodCallback& callback) {
  ReturnAsyncMethodResult(callback, false);
}

void FakeCryptohomeClient::AsyncMountGuest(
    const AsyncMethodCallback& callback) {
  ReturnAsyncMethodResult(callback, false);
}

void FakeCryptohomeClient::AsyncMountPublic(
    const std::string& public_mount_id,
    int flags,
    const AsyncMethodCallback& callback) {
  ReturnAsyncMethodResult(callback, false);
}

void FakeCryptohomeClient::TpmIsReady(
    const BoolDBusMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, true));
}

void FakeCryptohomeClient::TpmIsEnabled(
    const BoolDBusMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, true));
}

bool FakeCryptohomeClient::CallTpmIsEnabledAndBlock(bool* enabled) {
  *enabled = true;
  return true;
}

void FakeCryptohomeClient::TpmGetPassword(
    const StringDBusMethodCallback& callback) {
  const char kStubTpmPassword[] = "Stub-TPM-password";
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, DBUS_METHOD_CALL_SUCCESS,
                 std::string(kStubTpmPassword)));
}

void FakeCryptohomeClient::TpmIsOwned(
    const BoolDBusMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, true));
}

bool FakeCryptohomeClient::CallTpmIsOwnedAndBlock(bool* owned) {
  *owned = true;
  return true;
}

void FakeCryptohomeClient::TpmIsBeingOwned(
    const BoolDBusMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, true));
}

bool FakeCryptohomeClient::CallTpmIsBeingOwnedAndBlock(bool* owning) {
  *owning = true;
  return true;
}

void FakeCryptohomeClient::TpmCanAttemptOwnership(
    const VoidDBusMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS));
}

void FakeCryptohomeClient::TpmClearStoredPassword(
    const VoidDBusMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS));
}

bool FakeCryptohomeClient::CallTpmClearStoredPasswordAndBlock() {
  return true;
}

void FakeCryptohomeClient::Pkcs11IsTpmTokenReady(
    const BoolDBusMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, true));
}

void FakeCryptohomeClient::Pkcs11GetTpmTokenInfo(
    const Pkcs11GetTpmTokenInfoCallback& callback) {
  const char kStubTPMTokenName[] = "StubTPMTokenName";
  const char kStubUserPin[] = "012345";
  const int kStubSlot = 0;
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback,
                 DBUS_METHOD_CALL_SUCCESS,
                 std::string(kStubTPMTokenName),
                 std::string(kStubUserPin),
                 kStubSlot));
}

void FakeCryptohomeClient::Pkcs11GetTpmTokenInfoForUser(
    const std::string& username,
    const Pkcs11GetTpmTokenInfoCallback& callback) {
  Pkcs11GetTpmTokenInfo(callback);
}

bool FakeCryptohomeClient::InstallAttributesGet(const std::string& name,
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

bool FakeCryptohomeClient::InstallAttributesSet(
    const std::string& name,
    const std::vector<uint8>& value,
    bool* successful) {
  install_attrs_[name] = value;
  *successful = true;
  return true;
}

bool FakeCryptohomeClient::InstallAttributesFinalize(bool* successful) {
  locked_ = true;
  *successful = true;

  // Persist the install attributes so that they can be reloaded if the
  // browser is restarted. This is used for ease of development when device
  // enrollment is required.
  // The cryptohome::SerializedInstallAttributes protobuf lives in
  // chrome/browser/chromeos, so it can't be used directly here; use the
  // low-level protobuf API instead to just write the name-value pairs.
  // The cache file is read by EnterpriseInstallAttributes::ReadCacheFile.
  base::FilePath cache_path;
  if (!PathService::Get(chromeos::FILE_INSTALL_ATTRIBUTES, &cache_path))
    return false;

  std::string result;
  {
    // |result| can be used only after the StringOutputStream goes out of
    // scope.
    google::protobuf::io::StringOutputStream result_stream(&result);
    google::protobuf::io::CodedOutputStream result_output(&result_stream);

    // These tags encode a variable-length value on the wire, which can be
    // used to encode strings, bytes and messages. We only needs constants
    // for tag numbers 1 and 2 (see install_attributes.proto).
    const int kVarLengthTag1 = (1 << 3) | 0x2;
    const int kVarLengthTag2 = (2 << 3) | 0x2;

    typedef std::map<std::string, std::vector<uint8> >::const_iterator Iter;
    for (Iter it = install_attrs_.begin(); it != install_attrs_.end(); ++it) {
      std::string attr;
      {
        google::protobuf::io::StringOutputStream attr_stream(&attr);
        google::protobuf::io::CodedOutputStream attr_output(&attr_stream);

        attr_output.WriteVarint32(kVarLengthTag1);
        attr_output.WriteVarint32(it->first.size());
        attr_output.WriteString(it->first);
        attr_output.WriteVarint32(kVarLengthTag2);
        attr_output.WriteVarint32(it->second.size());
        attr_output.WriteRaw(it->second.data(), it->second.size());
      }

      // Two CodedOutputStreams are needed because inner messages must be
      // prefixed by their total length, which can't be easily computed before
      // writing their tags and values.
      result_output.WriteVarint32(kVarLengthTag2);
      result_output.WriteVarint32(attr.size());
      result_output.WriteRaw(attr.data(), attr.size());
    }
  }

  // The real implementation does a blocking wait on the dbus call; the fake
  // implementation must have this file written before returning.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  base::WriteFile(cache_path, result.data(), result.size());

  return true;
}

void FakeCryptohomeClient::InstallAttributesIsReady(
    const BoolDBusMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, true));
}

bool FakeCryptohomeClient::InstallAttributesIsInvalid(bool* is_invalid) {
  *is_invalid = false;
  return true;
}

bool FakeCryptohomeClient::InstallAttributesIsFirstInstall(
    bool* is_first_install) {
  *is_first_install = !locked_;
  return true;
}

void FakeCryptohomeClient::TpmAttestationIsPrepared(
    const BoolDBusMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, true));
}

void FakeCryptohomeClient::TpmAttestationIsEnrolled(
    const BoolDBusMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, true));
}

void FakeCryptohomeClient::AsyncTpmAttestationCreateEnrollRequest(
    chromeos::attestation::PrivacyCAType pca_type,
    const AsyncMethodCallback& callback) {
  ReturnAsyncMethodResult(callback, true);
}

void FakeCryptohomeClient::AsyncTpmAttestationEnroll(
    chromeos::attestation::PrivacyCAType pca_type,
    const std::string& pca_response,
    const AsyncMethodCallback& callback) {
  ReturnAsyncMethodResult(callback, false);
}

void FakeCryptohomeClient::AsyncTpmAttestationCreateCertRequest(
    chromeos::attestation::PrivacyCAType pca_type,
    attestation::AttestationCertificateProfile certificate_profile,
    const std::string& user_id,
    const std::string& request_origin,
    const AsyncMethodCallback& callback) {
  ReturnAsyncMethodResult(callback, true);
}

void FakeCryptohomeClient::AsyncTpmAttestationFinishCertRequest(
    const std::string& pca_response,
    attestation::AttestationKeyType key_type,
    const std::string& user_id,
    const std::string& key_name,
    const AsyncMethodCallback& callback) {
  ReturnAsyncMethodResult(callback, true);
}

void FakeCryptohomeClient::TpmAttestationDoesKeyExist(
    attestation::AttestationKeyType key_type,
    const std::string& user_id,
    const std::string& key_name,
    const BoolDBusMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, false));
}

void FakeCryptohomeClient::TpmAttestationGetCertificate(
    attestation::AttestationKeyType key_type,
    const std::string& user_id,
    const std::string& key_name,
    const DataMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, false, std::string()));
}

void FakeCryptohomeClient::TpmAttestationGetPublicKey(
    attestation::AttestationKeyType key_type,
    const std::string& user_id,
    const std::string& key_name,
    const DataMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, false, std::string()));
}

void FakeCryptohomeClient::TpmAttestationRegisterKey(
    attestation::AttestationKeyType key_type,
    const std::string& user_id,
    const std::string& key_name,
    const AsyncMethodCallback& callback) {
  ReturnAsyncMethodResult(callback, true);
}

void FakeCryptohomeClient::TpmAttestationSignEnterpriseChallenge(
    attestation::AttestationKeyType key_type,
    const std::string& user_id,
    const std::string& key_name,
    const std::string& domain,
    const std::string& device_id,
    attestation::AttestationChallengeOptions options,
    const std::string& challenge,
    const AsyncMethodCallback& callback) {
  ReturnAsyncMethodResult(callback, true);
}

void FakeCryptohomeClient::TpmAttestationSignSimpleChallenge(
    attestation::AttestationKeyType key_type,
    const std::string& user_id,
    const std::string& key_name,
    const std::string& challenge,
    const AsyncMethodCallback& callback) {
  ReturnAsyncMethodResult(callback, true);
}

void FakeCryptohomeClient::TpmAttestationGetKeyPayload(
    attestation::AttestationKeyType key_type,
    const std::string& user_id,
    const std::string& key_name,
    const DataMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, false, std::string()));
}

void FakeCryptohomeClient::TpmAttestationSetKeyPayload(
    attestation::AttestationKeyType key_type,
    const std::string& user_id,
    const std::string& key_name,
    const std::string& payload,
    const BoolDBusMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, false));
}

void FakeCryptohomeClient::TpmAttestationDeleteKeys(
    attestation::AttestationKeyType key_type,
    const std::string& user_id,
    const std::string& key_prefix,
    const BoolDBusMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, false));
}

void FakeCryptohomeClient::GetKeyDataEx(
    const cryptohome::AccountIdentifier& id,
    const cryptohome::AuthorizationRequest& auth,
    const cryptohome::GetKeyDataRequest& request,
    const ProtobufMethodCallback& callback) {
  cryptohome::BaseReply reply;
  reply.MutableExtension(cryptohome::GetKeyDataReply::reply);
  ReturnProtobufMethodCallback(reply, callback);
}

void FakeCryptohomeClient::CheckKeyEx(
    const cryptohome::AccountIdentifier& id,
    const cryptohome::AuthorizationRequest& auth,
    const cryptohome::CheckKeyRequest& request,
    const ProtobufMethodCallback& callback) {
  cryptohome::BaseReply reply;
  ReturnProtobufMethodCallback(reply, callback);
}

void FakeCryptohomeClient::MountEx(
    const cryptohome::AccountIdentifier& id,
    const cryptohome::AuthorizationRequest& auth,
    const cryptohome::MountRequest& request,
    const ProtobufMethodCallback& callback) {
  cryptohome::BaseReply reply;
  cryptohome::MountReply* mount =
      reply.MutableExtension(cryptohome::MountReply::reply);
  mount->set_sanitized_username(GetStubSanitizedUsername(id.email()));
  ReturnProtobufMethodCallback(reply, callback);
}

void FakeCryptohomeClient::AddKeyEx(
    const cryptohome::AccountIdentifier& id,
    const cryptohome::AuthorizationRequest& auth,
    const cryptohome::AddKeyRequest& request,
    const ProtobufMethodCallback& callback) {
  cryptohome::BaseReply reply;
  ReturnProtobufMethodCallback(reply, callback);
}

void FakeCryptohomeClient::RemoveKeyEx(
    const cryptohome::AccountIdentifier& id,
    const cryptohome::AuthorizationRequest& auth,
    const cryptohome::RemoveKeyRequest& request,
    const ProtobufMethodCallback& callback) {
  cryptohome::BaseReply reply;
  ReturnProtobufMethodCallback(reply, callback);
}

void FakeCryptohomeClient::UpdateKeyEx(
    const cryptohome::AccountIdentifier& id,
    const cryptohome::AuthorizationRequest& auth,
    const cryptohome::UpdateKeyRequest& request,
    const ProtobufMethodCallback& callback) {
  cryptohome::BaseReply reply;
  ReturnProtobufMethodCallback(reply, callback);
}

void FakeCryptohomeClient::GetBootAttribute(
    const cryptohome::GetBootAttributeRequest& request,
    const ProtobufMethodCallback& callback) {
  cryptohome::BaseReply reply;
  cryptohome::GetBootAttributeReply* attr_reply =
      reply.MutableExtension(cryptohome::GetBootAttributeReply::reply);
  attr_reply->set_value("");
  ReturnProtobufMethodCallback(reply, callback);
}

void FakeCryptohomeClient::SetBootAttribute(
    const cryptohome::SetBootAttributeRequest& request,
    const ProtobufMethodCallback& callback) {
  cryptohome::BaseReply reply;
  ReturnProtobufMethodCallback(reply, callback);
}

void FakeCryptohomeClient::FlushAndSignBootAttributes(
    const cryptohome::FlushAndSignBootAttributesRequest& request,
    const ProtobufMethodCallback& callback) {
  cryptohome::BaseReply reply;
  ReturnProtobufMethodCallback(reply, callback);
}

void FakeCryptohomeClient::SetServiceIsAvailable(bool is_available) {
  service_is_available_ = is_available;
  if (is_available) {
    std::vector<WaitForServiceToBeAvailableCallback> callbacks;
    callbacks.swap(pending_wait_for_service_to_be_available_callbacks_);
    for (size_t i = 0; i < callbacks.size(); ++i)
      callbacks[i].Run(is_available);
  }
}

// static
std::vector<uint8> FakeCryptohomeClient::GetStubSystemSalt() {
  const char kStubSystemSalt[] = "stub_system_salt";
  return std::vector<uint8>(kStubSystemSalt,
                            kStubSystemSalt + arraysize(kStubSystemSalt) - 1);
}

void FakeCryptohomeClient::ReturnProtobufMethodCallback(
    const cryptohome::BaseReply& reply,
    const ProtobufMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback,
                 DBUS_METHOD_CALL_SUCCESS,
                 true,
                 reply));
}

void FakeCryptohomeClient::ReturnAsyncMethodResult(
    const AsyncMethodCallback& callback,
    bool returns_data) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&FakeCryptohomeClient::ReturnAsyncMethodResultInternal,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback,
                 returns_data));
}

void FakeCryptohomeClient::ReturnAsyncMethodResultInternal(
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
