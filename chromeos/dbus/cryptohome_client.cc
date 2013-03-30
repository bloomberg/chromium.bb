// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/cryptohome_client.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/dbus/blocking_method_caller.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// The CryptohomeClient implementation.
class CryptohomeClientImpl : public CryptohomeClient {
 public:
  explicit CryptohomeClientImpl(dbus::Bus* bus)
      : proxy_(bus->GetObjectProxy(
            cryptohome::kCryptohomeServiceName,
            dbus::ObjectPath(cryptohome::kCryptohomeServicePath))),
        blocking_method_caller_(bus, proxy_),
        weak_ptr_factory_(this) {
    proxy_->ConnectToSignal(
        cryptohome::kCryptohomeInterface,
        cryptohome::kSignalAsyncCallStatus,
        base::Bind(&CryptohomeClientImpl::OnAsyncCallStatus,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&CryptohomeClientImpl::OnSignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));
    proxy_->ConnectToSignal(
        cryptohome::kCryptohomeInterface,
        cryptohome::kSignalAsyncCallStatusWithData,
        base::Bind(&CryptohomeClientImpl::OnAsyncCallStatusWithData,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&CryptohomeClientImpl::OnSignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  // CryptohomeClient override.
  virtual void SetAsyncCallStatusHandlers(
      const AsyncCallStatusHandler& handler,
      const AsyncCallStatusWithDataHandler& data_handler) OVERRIDE {
    async_call_status_handler_ = handler;
    async_call_status_data_handler_ = data_handler;
  }

  // CryptohomeClient override.
  virtual void ResetAsyncCallStatusHandlers() OVERRIDE {
    async_call_status_handler_.Reset();
    async_call_status_data_handler_.Reset();
  }

  // CryptohomeClient override.
  virtual void IsMounted(const BoolDBusMethodCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeIsMounted);
    CallBoolMethod(&method_call, callback);
  }

  // CryptohomeClient override.
  virtual bool Unmount(bool *success) OVERRIDE {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeUnmount);
    return CallBoolMethodAndBlock(&method_call, success);
  }

  // CryptohomeClient override.
  virtual void AsyncCheckKey(const std::string& username,
                             const std::string& key,
                             const AsyncMethodCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeAsyncCheckKey);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(username);
    writer.AppendString(key);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&CryptohomeClientImpl::OnAsyncMethodCall,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  callback));
  }

  // CryptohomeClient override.
  virtual void AsyncMigrateKey(const std::string& username,
                               const std::string& from_key,
                               const std::string& to_key,
                               const AsyncMethodCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeAsyncMigrateKey);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(username);
    writer.AppendString(from_key);
    writer.AppendString(to_key);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&CryptohomeClientImpl::OnAsyncMethodCall,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  callback));
  }

  // CryptohomeClient override.
  virtual void AsyncRemove(const std::string& username,
                           const AsyncMethodCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeAsyncRemove);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(username);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&CryptohomeClientImpl::OnAsyncMethodCall,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  callback));
  }

  // CryptohomeClient override.
  virtual bool GetSystemSalt(std::vector<uint8>* salt) OVERRIDE {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeGetSystemSalt);
    scoped_ptr<dbus::Response> response(
        blocking_method_caller_.CallMethodAndBlock(&method_call));
    if (!response.get())
      return false;
    dbus::MessageReader reader(response.get());
    uint8* bytes = NULL;
    size_t length = 0;
    if (!reader.PopArrayOfBytes(&bytes, &length))
      return false;
    salt->assign(bytes, bytes + length);
    return true;
  }

  // CryptohomeClient override,
  virtual void GetSanitizedUsername(
      const std::string& username,
      const StringDBusMethodCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeGetSanitizedUsername);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(username);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&CryptohomeClientImpl::OnStringMethod,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  callback));
  }

  // CryptohomeClient override.
  virtual void AsyncMount(const std::string& username,
                          const std::string& key,
                          int flags,
                          const AsyncMethodCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeAsyncMount);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(username);
    writer.AppendString(key);
    writer.AppendBool(flags & cryptohome::CREATE_IF_MISSING);
    writer.AppendBool(flags & cryptohome::ENSURE_EPHEMERAL);
    // deprecated_tracked_subdirectories
    writer.AppendArrayOfStrings(std::vector<std::string>());
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&CryptohomeClientImpl::OnAsyncMethodCall,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  callback));
  }

  // CryptohomeClient override.
  virtual void AsyncMountGuest(const AsyncMethodCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeAsyncMountGuest);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&CryptohomeClientImpl::OnAsyncMethodCall,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  callback));
  }

  // CryptohomeClient override.
  virtual void TpmIsReady(const BoolDBusMethodCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeTpmIsReady);
    CallBoolMethod(&method_call, callback);
  }

  // CryptohomeClient override.
  virtual void TpmIsEnabled(const BoolDBusMethodCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeTpmIsEnabled);
    CallBoolMethod(&method_call, callback);
  }

  // CryptohomeClient override.
  // TODO(hashimoto): Remove this method. crbug.com/141006
  virtual bool CallTpmIsEnabledAndBlock(bool* enabled) OVERRIDE {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeTpmIsEnabled);
    return CallBoolMethodAndBlock(&method_call, enabled);
  }

  // CryptohomeClient override.
  virtual void TpmGetPassword(
      const StringDBusMethodCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeTpmGetPassword);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&CryptohomeClientImpl::OnStringMethod,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback));
  }

  // CryptohomeClient override.
  virtual void TpmIsOwned(const BoolDBusMethodCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeTpmIsOwned);
    CallBoolMethod(&method_call, callback);
  }

  // CryptohomeClient override.
  // TODO(hashimoto): Remove this method. crbug.com/141012
  virtual bool CallTpmIsOwnedAndBlock(bool* owned) OVERRIDE {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeTpmIsOwned);
    return CallBoolMethodAndBlock(&method_call, owned);
  }

  // CryptohomeClient override.
  virtual void TpmIsBeingOwned(const BoolDBusMethodCallback& callback)
      OVERRIDE {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeTpmIsBeingOwned);
    CallBoolMethod(&method_call, callback);
  }

  // CryptohomeClient override.
  // TODO(hashimoto): Remove this method. crbug.com/141011
  virtual bool CallTpmIsBeingOwnedAndBlock(bool* owning) OVERRIDE {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeTpmIsBeingOwned);
    return CallBoolMethodAndBlock(&method_call, owning);
  }

  // CryptohomeClient override.
  virtual void TpmCanAttemptOwnership(
      const VoidDBusMethodCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeTpmCanAttemptOwnership);
    CallVoidMethod(&method_call, callback);
  }

  // CryptohomeClient overrides.
  virtual void TpmClearStoredPassword(const VoidDBusMethodCallback& callback)
      OVERRIDE {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeTpmClearStoredPassword);
    CallVoidMethod(&method_call, callback);
  }

  // CryptohomeClient override.
  // TODO(hashimoto): Remove this method. crbug.com/141010
  virtual bool CallTpmClearStoredPasswordAndBlock() OVERRIDE {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeTpmClearStoredPassword);
    scoped_ptr<dbus::Response> response(
        blocking_method_caller_.CallMethodAndBlock(&method_call));
    return response.get() != NULL;
  }

  // CryptohomeClient override.
  virtual void Pkcs11IsTpmTokenReady(const BoolDBusMethodCallback& callback)
      OVERRIDE {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomePkcs11IsTpmTokenReady);
    CallBoolMethod(&method_call, callback);
  }

  // CryptohomeClient override.
  virtual void Pkcs11GetTpmTokenInfo(
      const Pkcs11GetTpmTokenInfoCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomePkcs11GetTpmTokenInfo);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(
            &CryptohomeClientImpl::OnPkcs11GetTpmTokenInfo,
            weak_ptr_factory_.GetWeakPtr(),
            callback));
  }

  // CryptohomeClient override.
  virtual bool InstallAttributesGet(const std::string& name,
                                    std::vector<uint8>* value,
                                    bool* successful) OVERRIDE {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeInstallAttributesGet);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(name);
    scoped_ptr<dbus::Response> response(
        blocking_method_caller_.CallMethodAndBlock(&method_call));
    if (!response.get())
      return false;
    dbus::MessageReader reader(response.get());
    uint8* bytes = NULL;
    size_t length = 0;
    if (!reader.PopArrayOfBytes(&bytes, &length) ||
        !reader.PopBool(successful))
      return false;
    value->assign(bytes, bytes + length);
    return true;
  }

  // CryptohomeClient override.
  virtual bool InstallAttributesSet(const std::string& name,
                                    const std::vector<uint8>& value,
                                    bool* successful) OVERRIDE {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeInstallAttributesSet);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(name);
    writer.AppendArrayOfBytes(value.data(), value.size());
    return CallBoolMethodAndBlock(&method_call, successful);
  }

  // CryptohomeClient override.
  virtual bool InstallAttributesFinalize(bool* successful) OVERRIDE {
    dbus::MethodCall method_call(
        cryptohome::kCryptohomeInterface,
        cryptohome::kCryptohomeInstallAttributesFinalize);
    return CallBoolMethodAndBlock(&method_call, successful);
  }

  // CryptohomeClient override.
  virtual bool InstallAttributesIsReady(bool* is_ready) OVERRIDE {
    dbus::MethodCall method_call(
        cryptohome::kCryptohomeInterface,
        cryptohome::kCryptohomeInstallAttributesIsReady);
    return CallBoolMethodAndBlock(&method_call, is_ready);
  }

  // CryptohomeClient override.
  virtual bool InstallAttributesIsInvalid(bool* is_invalid) OVERRIDE {
    dbus::MethodCall method_call(
        cryptohome::kCryptohomeInterface,
        cryptohome::kCryptohomeInstallAttributesIsInvalid);
    return CallBoolMethodAndBlock(&method_call, is_invalid);
  }

  // CryptohomeClient override.
  virtual bool InstallAttributesIsFirstInstall(
      bool* is_first_install) OVERRIDE {
    dbus::MethodCall method_call(
        cryptohome::kCryptohomeInterface,
        cryptohome::kCryptohomeInstallAttributesIsFirstInstall);
    return CallBoolMethodAndBlock(&method_call, is_first_install);
  }

  // CryptohomeClient override.
  virtual void TpmAttestationIsPrepared(
        const BoolDBusMethodCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(
        cryptohome::kCryptohomeInterface,
        cryptohome::kCryptohomeTpmIsAttestationPrepared);
    return CallBoolMethod(&method_call, callback);
  }

  // CryptohomeClient override.
  virtual void TpmAttestationIsEnrolled(
        const BoolDBusMethodCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(
        cryptohome::kCryptohomeInterface,
        cryptohome::kCryptohomeTpmIsAttestationEnrolled);
    return CallBoolMethod(&method_call, callback);
  }

  // CryptohomeClient override.
  virtual void AsyncTpmAttestationCreateEnrollRequest(
      const AsyncMethodCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(
        cryptohome::kCryptohomeInterface,
        cryptohome::kCryptohomeAsyncTpmAttestationCreateEnrollRequest);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&CryptohomeClientImpl::OnAsyncMethodCall,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  callback));
  }

  // CryptohomeClient override.
  virtual void AsyncTpmAttestationEnroll(
      const std::string& pca_response,
      const AsyncMethodCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(
        cryptohome::kCryptohomeInterface,
        cryptohome::kCryptohomeAsyncTpmAttestationEnroll);
    dbus::MessageWriter writer(&method_call);
    writer.AppendArrayOfBytes(
        reinterpret_cast<const uint8*>(pca_response.data()),
        pca_response.size());
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&CryptohomeClientImpl::OnAsyncMethodCall,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  callback));
  }

  // CryptohomeClient override.
  virtual void AsyncTpmAttestationCreateCertRequest(
      int options,
      const AsyncMethodCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(
        cryptohome::kCryptohomeInterface,
        cryptohome::kCryptohomeAsyncTpmAttestationCreateCertRequest);
    dbus::MessageWriter writer(&method_call);
    bool include_stable_id = (options & INCLUDE_STABLE_ID);
    writer.AppendBool(include_stable_id);
    bool include_device_state = (options & INCLUDE_DEVICE_STATE);
    writer.AppendBool(include_device_state);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&CryptohomeClientImpl::OnAsyncMethodCall,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  callback));
  }

  // CryptohomeClient override.
  virtual void AsyncTpmAttestationFinishCertRequest(
      const std::string& pca_response,
      AttestationKeyType key_type,
      const std::string& key_name,
      const AsyncMethodCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(
        cryptohome::kCryptohomeInterface,
        cryptohome::kCryptohomeAsyncTpmAttestationFinishCertRequest);
    dbus::MessageWriter writer(&method_call);
    writer.AppendArrayOfBytes(
        reinterpret_cast<const uint8*>(pca_response.data()),
        pca_response.size());
    bool is_user_specific = (key_type == USER_KEY);
    writer.AppendBool(is_user_specific);
    writer.AppendString(key_name);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&CryptohomeClientImpl::OnAsyncMethodCall,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  callback));
  }

  // CryptohomeClient override.
  virtual void TpmAttestationDoesKeyExist(
      AttestationKeyType key_type,
      const std::string& key_name,
      const BoolDBusMethodCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(
        cryptohome::kCryptohomeInterface,
        cryptohome::kCryptohomeTpmAttestationDoesKeyExist);
    dbus::MessageWriter writer(&method_call);
    bool is_user_specific = (key_type == USER_KEY);
    writer.AppendBool(is_user_specific);
    writer.AppendString(key_name);
    CallBoolMethod(&method_call, callback);
  }

  // CryptohomeClient override.
  virtual void TpmAttestationGetCertificate(
      AttestationKeyType key_type,
      const std::string& key_name,
      const DataMethodCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(
        cryptohome::kCryptohomeInterface,
        cryptohome::kCryptohomeTpmAttestationGetCertificate);
    dbus::MessageWriter writer(&method_call);
    bool is_user_specific = (key_type == USER_KEY);
    writer.AppendBool(is_user_specific);
    writer.AppendString(key_name);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&CryptohomeClientImpl::OnDataMethod,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  callback));
  }

  // CryptohomeClient override.
  virtual void TpmAttestationGetPublicKey(
      AttestationKeyType key_type,
      const std::string& key_name,
      const DataMethodCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(
        cryptohome::kCryptohomeInterface,
        cryptohome::kCryptohomeTpmAttestationGetPublicKey);
    dbus::MessageWriter writer(&method_call);
    bool is_user_specific = (key_type == USER_KEY);
    writer.AppendBool(is_user_specific);
    writer.AppendString(key_name);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&CryptohomeClientImpl::OnDataMethod,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  callback));
  }

  // CryptohomeClient override.
  virtual void TpmAttestationRegisterKey(
      AttestationKeyType key_type,
      const std::string& key_name,
      const AsyncMethodCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(
        cryptohome::kCryptohomeInterface,
        cryptohome::kCryptohomeTpmAttestationRegisterKey);
    dbus::MessageWriter writer(&method_call);
    bool is_user_specific = (key_type == USER_KEY);
    writer.AppendBool(is_user_specific);
    writer.AppendString(key_name);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&CryptohomeClientImpl::OnAsyncMethodCall,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  callback));
  }

  // CryptohomeClient override.
  virtual void TpmAttestationSignEnterpriseChallenge(
      AttestationKeyType key_type,
      const std::string& key_name,
      const std::string& domain,
      const std::string& device_id,
      const std::string& challenge,
      const AsyncMethodCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(
        cryptohome::kCryptohomeInterface,
        cryptohome::kCryptohomeTpmAttestationSignEnterpriseChallenge);
    dbus::MessageWriter writer(&method_call);
    bool is_user_specific = (key_type == USER_KEY);
    writer.AppendBool(is_user_specific);
    writer.AppendString(key_name);
    writer.AppendString(domain);
    writer.AppendString(device_id);
    writer.AppendString(challenge);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&CryptohomeClientImpl::OnAsyncMethodCall,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  callback));
  }

  // CryptohomeClient override.
  virtual void TpmAttestationSignSimpleChallenge(
      AttestationKeyType key_type,
      const std::string& key_name,
      const std::string& challenge,
      const AsyncMethodCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(
        cryptohome::kCryptohomeInterface,
        cryptohome::kCryptohomeTpmAttestationSignEnterpriseChallenge);
    dbus::MessageWriter writer(&method_call);
    bool is_user_specific = (key_type == USER_KEY);
    writer.AppendBool(is_user_specific);
    writer.AppendString(key_name);
    writer.AppendString(challenge);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&CryptohomeClientImpl::OnAsyncMethodCall,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  callback));
  }

 private:
  // Handles the result of AsyncXXX methods.
  void OnAsyncMethodCall(const AsyncMethodCallback& callback,
                         dbus::Response* response) {
    if (!response)
      return;
    dbus::MessageReader reader(response);
    int async_id = 0;
    if (!reader.PopInt32(&async_id)) {
      LOG(ERROR) << "Invalid response: " << response->ToString();
      return;
    }
    callback.Run(async_id);
  }

  // Calls a method without result values.
  void CallVoidMethod(dbus::MethodCall* method_call,
                      const VoidDBusMethodCallback& callback) {
    proxy_->CallMethod(method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&CryptohomeClientImpl::OnVoidMethod,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  callback));
  }

  void OnVoidMethod(const VoidDBusMethodCallback& callback,
                    dbus::Response* response) {
    if (!response) {
      callback.Run(DBUS_METHOD_CALL_FAILURE);
      return;
    }
    callback.Run(DBUS_METHOD_CALL_SUCCESS);
  }

  // Calls a method with a bool value reult and block.
  bool CallBoolMethodAndBlock(dbus::MethodCall* method_call,
                              bool* result) {
    scoped_ptr<dbus::Response> response(
        blocking_method_caller_.CallMethodAndBlock(method_call));
    if (!response.get())
      return false;
    dbus::MessageReader reader(response.get());
    return reader.PopBool(result);
  }

  // Calls a method with a bool value result.
  void CallBoolMethod(dbus::MethodCall* method_call,
                      const BoolDBusMethodCallback& callback) {
    proxy_->CallMethod(method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(
                           &CryptohomeClientImpl::OnBoolMethod,
                           weak_ptr_factory_.GetWeakPtr(),
                           callback));
  }

  // Handles responses for methods with a bool value result.
  void OnBoolMethod(const BoolDBusMethodCallback& callback,
                    dbus::Response* response) {
    if (!response) {
      callback.Run(DBUS_METHOD_CALL_FAILURE, false);
      return;
    }
    dbus::MessageReader reader(response);
    bool result = false;
    if (!reader.PopBool(&result)) {
      callback.Run(DBUS_METHOD_CALL_FAILURE, false);
      return;
    }
    callback.Run(DBUS_METHOD_CALL_SUCCESS, result);
  }

  // Handles responses for methods with a string value result.
  void OnStringMethod(const StringDBusMethodCallback& callback,
                      dbus::Response* response) {
    if (!response) {
      callback.Run(DBUS_METHOD_CALL_FAILURE, std::string());
      return;
    }
    dbus::MessageReader reader(response);
    std::string result;
    if (!reader.PopString(&result)) {
      callback.Run(DBUS_METHOD_CALL_FAILURE, std::string());
      return;
    }
    callback.Run(DBUS_METHOD_CALL_SUCCESS, result);
  }

  // Handles responses for methods with a bool result and data.
  void OnDataMethod(const DataMethodCallback& callback,
                    dbus::Response* response) {
    if (!response) {
      callback.Run(DBUS_METHOD_CALL_FAILURE, false, std::string());
      return;
    }
    dbus::MessageReader reader(response);
    bool result = false;
    if (!reader.PopBool(&result)) {
      callback.Run(DBUS_METHOD_CALL_FAILURE, false, std::string());
      return;
    }
    std::string data;
    if (!reader.PopString(&data)) {
      callback.Run(DBUS_METHOD_CALL_FAILURE, false, std::string());
      return;
    }
    callback.Run(DBUS_METHOD_CALL_SUCCESS, result, data);
  }

  // Handles responses for Pkcs11GetTpmtTokenInfo.
  void OnPkcs11GetTpmTokenInfo(const Pkcs11GetTpmTokenInfoCallback& callback,
                               dbus::Response* response) {
    if (!response) {
      callback.Run(DBUS_METHOD_CALL_FAILURE, std::string(), std::string());
      return;
    }
    dbus::MessageReader reader(response);
    std::string label;
    std::string user_pin;
    if (!reader.PopString(&label) || !reader.PopString(&user_pin)) {
      callback.Run(DBUS_METHOD_CALL_FAILURE, std::string(), std::string());
      return;
    }
    callback.Run(DBUS_METHOD_CALL_SUCCESS, label, user_pin);
  }

  // Handles AsyncCallStatus signal.
  void OnAsyncCallStatus(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    int async_id = 0;
    bool return_status = false;
    int return_code = 0;
    if (!reader.PopInt32(&async_id) ||
        !reader.PopBool(&return_status) ||
        !reader.PopInt32(&return_code)) {
      LOG(ERROR) << "Invalid signal: " << signal->ToString();
      return;
    }
    if (!async_call_status_handler_.is_null())
      async_call_status_handler_.Run(async_id, return_status, return_code);
  }

  // Handles AsyncCallStatusWithData signal.
  void OnAsyncCallStatusWithData(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    int async_id = 0;
    bool return_status = false;
    uint8* return_data_buffer = NULL;
    size_t return_data_length = 0;
    if (!reader.PopInt32(&async_id) ||
        !reader.PopBool(&return_status) ||
        !reader.PopArrayOfBytes(&return_data_buffer, &return_data_length)) {
      LOG(ERROR) << "Invalid signal: " << signal->ToString();
      return;
    }
    if (!async_call_status_data_handler_.is_null()) {
      std::string return_data(reinterpret_cast<char*>(return_data_buffer),
                              return_data_length);
      async_call_status_data_handler_.Run(async_id, return_status, return_data);
    }
  }

  // Handles the result of signal connection setup.
  void OnSignalConnected(const std::string& interface,
                         const std::string& signal,
                         bool succeeded) {
    LOG_IF(ERROR, !succeeded) << "Connect to " << interface << " " <<
        signal << " failed.";
  }

  dbus::ObjectProxy* proxy_;
  BlockingMethodCaller blocking_method_caller_;
  AsyncCallStatusHandler async_call_status_handler_;
  AsyncCallStatusWithDataHandler async_call_status_data_handler_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<CryptohomeClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CryptohomeClientImpl);
};

// A stub implementaion of CryptohomeClient.
class CryptohomeClientStubImpl : public CryptohomeClient {
 public:
  CryptohomeClientStubImpl()
      : async_call_id_(1),
        tpm_is_ready_counter_(0),
        locked_(false),
        weak_ptr_factory_(this) {
  }

  virtual ~CryptohomeClientStubImpl() {}

  // CryptohomeClient override.
  virtual void SetAsyncCallStatusHandlers(
      const AsyncCallStatusHandler& handler,
      const AsyncCallStatusWithDataHandler& data_handler) OVERRIDE {
    async_call_status_handler_ = handler;
    async_call_status_data_handler_ = data_handler;
  }

  // CryptohomeClient override.
  virtual void ResetAsyncCallStatusHandlers() OVERRIDE {
    async_call_status_handler_.Reset();
    async_call_status_data_handler_.Reset();
  }

  // CryptohomeClient override.
  virtual void IsMounted(const BoolDBusMethodCallback& callback) OVERRIDE {
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, true));
  }

  // CryptohomeClient override.
  virtual bool Unmount(bool* success) OVERRIDE {
    *success = true;
    return true;
  }

  // CryptohomeClient override.
  virtual void AsyncCheckKey(const std::string& username,
                             const std::string& key,
                             const AsyncMethodCallback& callback) OVERRIDE {
    ReturnAsyncMethodResult(callback, false);
  }

  // CryptohomeClient override.
  virtual void AsyncMigrateKey(const std::string& username,
                               const std::string& from_key,
                               const std::string& to_key,
                               const AsyncMethodCallback& callback) OVERRIDE {
    ReturnAsyncMethodResult(callback, false);
  }

  // CryptohomeClient override.
  virtual void AsyncRemove(const std::string& username,
                           const AsyncMethodCallback& callback) OVERRIDE {
    ReturnAsyncMethodResult(callback, false);
  }

  // CryptohomeClient override.
  virtual bool GetSystemSalt(std::vector<uint8>* salt) OVERRIDE {
    const char kStubSystemSalt[] = "stub_system_salt";
    salt->assign(kStubSystemSalt,
                 kStubSystemSalt + arraysize(kStubSystemSalt) - 1);
    return true;
  }

  // CryptohomeClient override.
  virtual void GetSanitizedUsername(
      const std::string& username,
      const StringDBusMethodCallback& callback) OVERRIDE {
    // Even for stub implementation we have to return different values
    // so that multi-profiles would work.
    std::string sanitized_username = username + "-profile";
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, sanitized_username));
  }

  // CryptohomeClient override.
  virtual void AsyncMount(const std::string& username,
                          const std::string& key,
                          int flags,
                          const AsyncMethodCallback& callback) OVERRIDE {
    ReturnAsyncMethodResult(callback, false);
  }

  // CryptohomeClient override.
  virtual void AsyncMountGuest(const AsyncMethodCallback& callback) OVERRIDE {
    ReturnAsyncMethodResult(callback, false);
  }

  // CryptohomeClient override.
  virtual void TpmIsReady(const BoolDBusMethodCallback& callback) OVERRIDE {
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, true));
  }

  // CryptohomeClient override.
  virtual void TpmIsEnabled(const BoolDBusMethodCallback& callback) OVERRIDE {
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, true));
  }

  // CryptohomeClient override.
  virtual bool CallTpmIsEnabledAndBlock(bool* enabled) OVERRIDE {
    *enabled = true;
    return true;
  }

  // CryptohomeClient override.
  virtual void TpmGetPassword(
      const StringDBusMethodCallback& callback) OVERRIDE {
    const char kStubTpmPassword[] = "Stub-TPM-password";
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, kStubTpmPassword));
  }

  // CryptohomeClient override.
  virtual void TpmIsOwned(const BoolDBusMethodCallback& callback) OVERRIDE {
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, true));
  }

  // CryptohomeClient override.
  virtual bool CallTpmIsOwnedAndBlock(bool* owned) OVERRIDE {
    *owned = true;
    return true;
  }

  // CryptohomeClient override.
  virtual void TpmIsBeingOwned(const BoolDBusMethodCallback& callback)
      OVERRIDE {
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, true));
  }

  // CryptohomeClient override.
  virtual bool CallTpmIsBeingOwnedAndBlock(bool* owning) OVERRIDE {
    *owning = true;
    return true;
  }

  // CryptohomeClient override.
  virtual void TpmCanAttemptOwnership(
      const VoidDBusMethodCallback& callback) OVERRIDE {
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS));
  }

  // CryptohomeClient override.
  virtual void TpmClearStoredPassword(const VoidDBusMethodCallback& callback)
      OVERRIDE {
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS));
  }

  // CryptohomeClient override.
  virtual bool CallTpmClearStoredPasswordAndBlock() OVERRIDE { return true; }

  // CryptohomeClient override.
  virtual void Pkcs11IsTpmTokenReady(
      const BoolDBusMethodCallback& callback) OVERRIDE {
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, true));
  }

  // CryptohomeClient override.
  virtual void Pkcs11GetTpmTokenInfo(
      const Pkcs11GetTpmTokenInfoCallback& callback) OVERRIDE {
    const char kStubLabel[] = "Stub TPM Token";
    const char kStubUserPin[] = "012345";
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, kStubLabel,
                              kStubUserPin));
  }

  // CryptohomeClient override.
  virtual bool InstallAttributesGet(const std::string& name,
                                    std::vector<uint8>* value,
                                    bool* successful) OVERRIDE {
    if (install_attrs_.find(name) != install_attrs_.end()) {
      *value = install_attrs_[name];
      *successful = true;
    } else {
      value->clear();
      *successful = false;
    }
    return true;
  }

  // CryptohomeClient override.
  virtual bool InstallAttributesSet(const std::string& name,
                                    const std::vector<uint8>& value,
                                    bool* successful) OVERRIDE {
    install_attrs_[name] = value;
    *successful = true;
    return true;
  }

  // CryptohomeClient override.
  virtual bool InstallAttributesFinalize(bool* successful) OVERRIDE {
    locked_ = true;
    *successful = true;
    return true;
  }

  // CryptohomeClient override.
  virtual bool InstallAttributesIsReady(bool* is_ready) OVERRIDE {
    *is_ready = true;
    return true;
  }

  // CryptohomeClient override.
  virtual bool InstallAttributesIsInvalid(bool* is_invalid) OVERRIDE {
    *is_invalid = false;
    return true;
  }

  // CryptohomeClient override.
  virtual bool InstallAttributesIsFirstInstall(
      bool* is_first_install) OVERRIDE {
    *is_first_install = !locked_;
    return true;
  }

  // CryptohomeClient override.
  virtual void TpmAttestationIsPrepared(
        const BoolDBusMethodCallback& callback) OVERRIDE {
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, true));
  }

  // CryptohomeClient override.
  virtual void TpmAttestationIsEnrolled(
        const BoolDBusMethodCallback& callback) OVERRIDE {
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, true));
  }

  // CryptohomeClient override.
  virtual void AsyncTpmAttestationCreateEnrollRequest(
      const AsyncMethodCallback& callback) OVERRIDE {
    ReturnAsyncMethodResult(callback, true);
  }

  // CryptohomeClient override.
  virtual void AsyncTpmAttestationEnroll(
      const std::string& pca_response,
      const AsyncMethodCallback& callback) OVERRIDE {
    ReturnAsyncMethodResult(callback, false);
  }

  // CryptohomeClient override.
  virtual void AsyncTpmAttestationCreateCertRequest(
      int options,
      const AsyncMethodCallback& callback) OVERRIDE {
    ReturnAsyncMethodResult(callback, true);
  }

  // CryptohomeClient override.
  virtual void AsyncTpmAttestationFinishCertRequest(
      const std::string& pca_response,
      AttestationKeyType key_type,
      const std::string& key_name,
      const AsyncMethodCallback& callback) OVERRIDE {
    ReturnAsyncMethodResult(callback, true);
  }

  // CryptohomeClient override.
  virtual void TpmAttestationDoesKeyExist(
      AttestationKeyType key_type,
      const std::string& key_name,
      const BoolDBusMethodCallback& callback) OVERRIDE {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, false));
  }

  // CryptohomeClient override.
  virtual void TpmAttestationGetCertificate(
      AttestationKeyType key_type,
      const std::string& key_name,
      const DataMethodCallback& callback) OVERRIDE {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, false, std::string()));
  }

  // CryptohomeClient override.
  virtual void TpmAttestationGetPublicKey(
      AttestationKeyType key_type,
      const std::string& key_name,
      const DataMethodCallback& callback) OVERRIDE {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, false, std::string()));
  }

  // CryptohomeClient override.
  virtual void TpmAttestationRegisterKey(
      AttestationKeyType key_type,
      const std::string& key_name,
      const AsyncMethodCallback& callback) OVERRIDE {
    ReturnAsyncMethodResult(callback, true);
  }

  // CryptohomeClient override.
  virtual void TpmAttestationSignEnterpriseChallenge(
      AttestationKeyType key_type,
      const std::string& key_name,
      const std::string& domain,
      const std::string& device_id,
      const std::string& challenge,
      const AsyncMethodCallback& callback) OVERRIDE {
    ReturnAsyncMethodResult(callback, true);
  }

  // CryptohomeClient override.
  virtual void TpmAttestationSignSimpleChallenge(
      AttestationKeyType key_type,
      const std::string& key_name,
      const std::string& challenge,
      const AsyncMethodCallback& callback) OVERRIDE {
    ReturnAsyncMethodResult(callback, true);
  }

 private:
  // Posts tasks which return fake results to the UI thread.
  void ReturnAsyncMethodResult(const AsyncMethodCallback& callback,
                               bool returns_data) {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&CryptohomeClientStubImpl::ReturnAsyncMethodResultInternal,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback,
                   returns_data));
  }

  // This method is used to implement ReturnAsyncMethodResult.
  void ReturnAsyncMethodResultInternal(const AsyncMethodCallback& callback,
                                       bool returns_data) {
    callback.Run(async_call_id_);
    if (!returns_data && !async_call_status_handler_.is_null()) {
      MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(async_call_status_handler_,
                     async_call_id_,
                     true,
                     cryptohome::MOUNT_ERROR_NONE));
    } else if (returns_data && !async_call_status_data_handler_.is_null()) {
      MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(async_call_status_data_handler_,
                     async_call_id_,
                     true,
                     std::string()));
    }
    ++async_call_id_;
  }

  int async_call_id_;
  AsyncCallStatusHandler async_call_status_handler_;
  AsyncCallStatusWithDataHandler async_call_status_data_handler_;
  int tpm_is_ready_counter_;
  std::map<std::string, std::vector<uint8> > install_attrs_;
  bool locked_;
  base::WeakPtrFactory<CryptohomeClientStubImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CryptohomeClientStubImpl);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// CryptohomeClient

CryptohomeClient::CryptohomeClient() {}

CryptohomeClient::~CryptohomeClient() {}

// static
CryptohomeClient* CryptohomeClient::Create(DBusClientImplementationType type,
                                           dbus::Bus* bus) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION)
    return new CryptohomeClientImpl(bus);
  DCHECK_EQ(STUB_DBUS_CLIENT_IMPLEMENTATION, type);
  return new CryptohomeClientStubImpl();
}

}  // namespace chromeos
