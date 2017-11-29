// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/session_manager_client.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/chromeos_paths.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/blocking_method_caller.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/login_manager/arc.pb.h"
#include "chromeos/dbus/login_manager/policy_descriptor.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "crypto/sha2.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "dbus/scoped_dbus_error.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

using RetrievePolicyResponseType =
    SessionManagerClient::RetrievePolicyResponseType;
using login_manager::PolicyDescriptor;

constexpr char kStubPolicyFile[] = "stub_policy";
constexpr char kStubDevicePolicyFile[] = "stub_device_policy";
constexpr char kStubStateKeysFile[] = "stub_state_keys";

constexpr char kEmptyAccountId[] = "";

// Returns a location for |file| that is specific to the given |cryptohome_id|.
// These paths will be relative to DIR_USER_POLICY_KEYS, and can be used only
// to store stub files.
base::FilePath GetUserFilePath(const cryptohome::Identification& cryptohome_id,
                               const char* file) {
  base::FilePath keys_path;
  if (!PathService::Get(chromeos::DIR_USER_POLICY_KEYS, &keys_path))
    return base::FilePath();
  const std::string sanitized =
      CryptohomeClient::GetStubSanitizedUsername(cryptohome_id);
  return keys_path.AppendASCII(sanitized).AppendASCII(file);
}

// Helper to asynchronously retrieve a file's content.
std::string GetFileContent(const base::FilePath& path) {
  std::string result;
  if (!path.empty())
    base::ReadFileToString(path, &result);
  return result;
}

// Helper to write a file in a background thread.
void StoreFile(const base::FilePath& path, const std::string& data) {
  const int size = static_cast<int>(data.size());
  if (path.empty() ||
      !base::CreateDirectory(path.DirName()) ||
      base::WriteFile(path, data.data(), size) != size) {
    LOG(WARNING) << "Failed to write to " << path.value();
  }
}

// Helper to asynchronously read (or if missing create) state key stubs.
std::vector<std::string> ReadCreateStateKeysStub(const base::FilePath& path) {
  std::string contents;
  if (base::PathExists(path)) {
    contents = GetFileContent(path);
  } else {
    // Create stub state keys on the fly.
    for (int i = 0; i < 5; ++i) {
      contents += crypto::SHA256HashString(
          base::IntToString(i) +
          base::Int64ToString(base::Time::Now().ToJavaTime()));
    }
    StoreFile(path, contents);
  }

  std::vector<std::string> state_keys;
  for (size_t i = 0; i < contents.size() / 32; ++i) {
    state_keys.push_back(contents.substr(i * 32, 32));
  }
  return state_keys;
}

// Helper to notify the callback with SUCCESS, to be used by the stub.
void NotifyOnRetrievePolicySuccess(
    SessionManagerClient::RetrievePolicyCallback callback,
    const std::string& protobuf) {
  std::move(callback).Run(RetrievePolicyResponseType::SUCCESS, protobuf);
}

// Helper to get the enum type of RetrievePolicyResponseType based on error
// name.
RetrievePolicyResponseType GetPolicyResponseTypeByError(
    base::StringPiece error_name) {
  if (error_name == login_manager::dbus_error::kNone) {
    return RetrievePolicyResponseType::SUCCESS;
  } else if (error_name == login_manager::dbus_error::kGetServiceFail ||
             error_name == login_manager::dbus_error::kSessionDoesNotExist) {
    // TODO(crbug.com/765644, ljusten): Remove kSessionDoesNotExist case once
    // Chrome OS has switched to kGetServiceFail.
    return RetrievePolicyResponseType::GET_SERVICE_FAIL;
  } else if (error_name == login_manager::dbus_error::kSigEncodeFail) {
    return RetrievePolicyResponseType::POLICY_ENCODE_ERROR;
  }
  return RetrievePolicyResponseType::OTHER_ERROR;
}

// Logs UMA stat for retrieve policy request, corresponding to D-Bus method name
// used.
void LogPolicyResponseUma(login_manager::PolicyAccountType account_type,
                          RetrievePolicyResponseType response) {
  switch (account_type) {
    case login_manager::ACCOUNT_TYPE_DEVICE:
      UMA_HISTOGRAM_ENUMERATION("Enterprise.RetrievePolicyResponse.Device",
                                response, RetrievePolicyResponseType::COUNT);
      break;
    case login_manager::ACCOUNT_TYPE_DEVICE_LOCAL_ACCOUNT:
      UMA_HISTOGRAM_ENUMERATION(
          "Enterprise.RetrievePolicyResponse.DeviceLocalAccount", response,
          RetrievePolicyResponseType::COUNT);
      break;
    case login_manager::ACCOUNT_TYPE_USER:
      UMA_HISTOGRAM_ENUMERATION("Enterprise.RetrievePolicyResponse.User",
                                response, RetrievePolicyResponseType::COUNT);
      break;
    case login_manager::ACCOUNT_TYPE_SESSIONLESS_USER:
      UMA_HISTOGRAM_ENUMERATION(
          "Enterprise.RetrievePolicyResponse.UserDuringLogin", response,
          RetrievePolicyResponseType::COUNT);
      break;
  }
}

PolicyDescriptor MakePolicyDescriptor(
    login_manager::PolicyAccountType account_type,
    const std::string& account_id) {
  PolicyDescriptor descriptor;
  descriptor.set_account_type(account_type);
  descriptor.set_account_id(account_id);
  descriptor.set_domain(login_manager::POLICY_DOMAIN_CHROME);
  return descriptor;
}

}  // namespace

// The SessionManagerClient implementation used in production.
class SessionManagerClientImpl : public SessionManagerClient {
 public:
  SessionManagerClientImpl() : weak_ptr_factory_(this) {}

  ~SessionManagerClientImpl() override = default;

  // SessionManagerClient overrides:
  void SetStubDelegate(StubDelegate* delegate) override {
    // Do nothing; this isn't a stub implementation.
  }

  void AddObserver(Observer* observer) override {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) override {
    observers_.RemoveObserver(observer);
  }

  bool HasObserver(const Observer* observer) const override {
    return observers_.HasObserver(observer);
  }

  bool IsScreenLocked() const override { return screen_is_locked_; }

  void EmitLoginPromptVisible() override {
    SimpleMethodCallToSessionManager(
        login_manager::kSessionManagerEmitLoginPromptVisible);
    for (auto& observer : observers_)
      observer.EmitLoginPromptVisibleCalled();
  }

  void RestartJob(int socket_fd,
                  const std::vector<std::string>& argv,
                  VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(login_manager::kSessionManagerInterface,
                                 login_manager::kSessionManagerRestartJob);
    dbus::MessageWriter writer(&method_call);
    writer.AppendFileDescriptor(socket_fd);
    writer.AppendArrayOfStrings(argv);
    session_manager_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&SessionManagerClientImpl::OnVoidMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void StartSession(const cryptohome::Identification& cryptohome_id) override {
    dbus::MethodCall method_call(login_manager::kSessionManagerInterface,
                                 login_manager::kSessionManagerStartSession);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(cryptohome_id.id());
    writer.AppendString("");  // Unique ID is deprecated
    session_manager_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        dbus::ObjectProxy::EmptyResponseCallback());
  }

  void StopSession() override {
    dbus::MethodCall method_call(login_manager::kSessionManagerInterface,
                                 login_manager::kSessionManagerStopSession);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString("");  // Unique ID is deprecated
    session_manager_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        dbus::ObjectProxy::EmptyResponseCallback());
  }

  void StartDeviceWipe() override {
    SimpleMethodCallToSessionManager(
        login_manager::kSessionManagerStartDeviceWipe);
  }

  void StartTPMFirmwareUpdate(const std::string& update_mode) override {
    dbus::MethodCall method_call(
        login_manager::kSessionManagerInterface,
        login_manager::kSessionManagerStartTPMFirmwareUpdate);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(update_mode);
    session_manager_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        dbus::ObjectProxy::EmptyResponseCallback());
  }

  void RequestLockScreen() override {
    SimpleMethodCallToSessionManager(login_manager::kSessionManagerLockScreen);
  }

  void NotifyLockScreenShown() override {
    SimpleMethodCallToSessionManager(
        login_manager::kSessionManagerHandleLockScreenShown);
  }

  void NotifyLockScreenDismissed() override {
    SimpleMethodCallToSessionManager(
        login_manager::kSessionManagerHandleLockScreenDismissed);
  }

  void NotifySupervisedUserCreationStarted() override {
    SimpleMethodCallToSessionManager(
        login_manager::kSessionManagerHandleSupervisedUserCreationStarting);
  }

  void NotifySupervisedUserCreationFinished() override {
    SimpleMethodCallToSessionManager(
        login_manager::kSessionManagerHandleSupervisedUserCreationFinished);
  }

  void RetrieveActiveSessions(ActiveSessionsCallback callback) override {
    dbus::MethodCall method_call(
        login_manager::kSessionManagerInterface,
        login_manager::kSessionManagerRetrieveActiveSessions);

    session_manager_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&SessionManagerClientImpl::OnRetrieveActiveSessions,
                       weak_ptr_factory_.GetWeakPtr(),
                       login_manager::kSessionManagerRetrieveActiveSessions,
                       std::move(callback)));
  }

  void RetrieveDevicePolicy(RetrievePolicyCallback callback) override {
    PolicyDescriptor descriptor = MakePolicyDescriptor(
        login_manager::ACCOUNT_TYPE_DEVICE, kEmptyAccountId);
    CallRetrievePolicy(descriptor, std::move(callback));
  }

  RetrievePolicyResponseType BlockingRetrieveDevicePolicy(
      std::string* policy_out) override {
    PolicyDescriptor descriptor = MakePolicyDescriptor(
        login_manager::ACCOUNT_TYPE_DEVICE, kEmptyAccountId);
    return BlockingRetrievePolicy(descriptor, policy_out);
  }

  void RetrievePolicyForUser(const cryptohome::Identification& cryptohome_id,
                             RetrievePolicyCallback callback) override {
    PolicyDescriptor descriptor = MakePolicyDescriptor(
        login_manager::ACCOUNT_TYPE_USER, cryptohome_id.id());
    CallRetrievePolicy(descriptor, std::move(callback));
  }

  RetrievePolicyResponseType BlockingRetrievePolicyForUser(
      const cryptohome::Identification& cryptohome_id,
      std::string* policy_out) override {
    PolicyDescriptor descriptor = MakePolicyDescriptor(
        login_manager::ACCOUNT_TYPE_USER, cryptohome_id.id());
    return BlockingRetrievePolicy(descriptor, policy_out);
  }

  void RetrievePolicyForUserWithoutSession(
      const cryptohome::Identification& cryptohome_id,
      RetrievePolicyCallback callback) override {
    PolicyDescriptor descriptor = MakePolicyDescriptor(
        login_manager::ACCOUNT_TYPE_SESSIONLESS_USER, cryptohome_id.id());
    CallRetrievePolicy(descriptor, std::move(callback));
  }

  void RetrieveDeviceLocalAccountPolicy(
      const std::string& account_name,
      RetrievePolicyCallback callback) override {
    PolicyDescriptor descriptor = MakePolicyDescriptor(
        login_manager::ACCOUNT_TYPE_DEVICE_LOCAL_ACCOUNT, account_name);
    CallRetrievePolicy(descriptor, std::move(callback));
  }

  RetrievePolicyResponseType BlockingRetrieveDeviceLocalAccountPolicy(
      const std::string& account_name,
      std::string* policy_out) override {
    PolicyDescriptor descriptor = MakePolicyDescriptor(
        login_manager::ACCOUNT_TYPE_DEVICE_LOCAL_ACCOUNT, account_name);
    return BlockingRetrievePolicy(descriptor, policy_out);
  }

  void StoreDevicePolicy(const std::string& policy_blob,
                         VoidDBusMethodCallback callback) override {
    PolicyDescriptor descriptor = MakePolicyDescriptor(
        login_manager::ACCOUNT_TYPE_DEVICE, kEmptyAccountId);
    CallStorePolicy(descriptor, policy_blob, std::move(callback));
  }

  void StorePolicyForUser(const cryptohome::Identification& cryptohome_id,
                          const std::string& policy_blob,
                          VoidDBusMethodCallback callback) override {
    PolicyDescriptor descriptor = MakePolicyDescriptor(
        login_manager::ACCOUNT_TYPE_USER, cryptohome_id.id());
    CallStorePolicy(descriptor, policy_blob, std::move(callback));
  }

  void StoreDeviceLocalAccountPolicy(const std::string& account_name,
                                     const std::string& policy_blob,
                                     VoidDBusMethodCallback callback) override {
    PolicyDescriptor descriptor = MakePolicyDescriptor(
        login_manager::ACCOUNT_TYPE_DEVICE_LOCAL_ACCOUNT, account_name);
    CallStorePolicy(descriptor, policy_blob, std::move(callback));
  }

  bool SupportsRestartToApplyUserFlags() const override { return true; }

  void SetFlagsForUser(const cryptohome::Identification& cryptohome_id,
                       const std::vector<std::string>& flags) override {
    dbus::MethodCall method_call(login_manager::kSessionManagerInterface,
                                 login_manager::kSessionManagerSetFlagsForUser);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(cryptohome_id.id());
    writer.AppendArrayOfStrings(flags);
    session_manager_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        dbus::ObjectProxy::EmptyResponseCallback());
  }

  void GetServerBackedStateKeys(StateKeysCallback callback) override {
    dbus::MethodCall method_call(
        login_manager::kSessionManagerInterface,
        login_manager::kSessionManagerGetServerBackedStateKeys);

    // Infinite timeout needed because the state keys are not generated as long
    // as the time sync hasn't been done (which requires network).
    // TODO(igorcov): Since this is a resource allocated that could last a long
    // time, we will need to change the behavior to either listen to
    // LastSyncInfo event from tlsdated or communicate through signals with
    // session manager in this particular flow.
    session_manager_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_INFINITE,
        base::BindOnce(&SessionManagerClientImpl::OnGetServerBackedStateKeys,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void StartArcInstance(ArcStartupMode startup_mode,
                        const cryptohome::Identification& cryptohome_id,
                        bool skip_boot_completed_broadcast,
                        bool scan_vendor_priv_app,
                        bool native_bridge_experiment,
                        StartArcInstanceCallback callback) override {
    DCHECK(!callback.is_null());

    dbus::MethodCall method_call(
        login_manager::kSessionManagerInterface,
        login_manager::kSessionManagerStartArcInstance);
    dbus::MessageWriter writer(&method_call);

    login_manager::StartArcInstanceRequest request;
    request.set_native_bridge_experiment(native_bridge_experiment);
    switch (startup_mode) {
      case ArcStartupMode::FULL:
        request.set_account_id(cryptohome_id.id());
        request.set_skip_boot_completed_broadcast(
            skip_boot_completed_broadcast);
        request.set_scan_vendor_priv_app(scan_vendor_priv_app);
        break;
      case ArcStartupMode::LOGIN_SCREEN:
        request.set_for_login_screen(true);
        break;
    }
    writer.AppendProtoAsArrayOfBytes(request);

    session_manager_proxy_->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&SessionManagerClientImpl::OnStartArcInstance,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void StopArcInstance(VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(login_manager::kSessionManagerInterface,
                                 login_manager::kSessionManagerStopArcInstance);
    session_manager_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&SessionManagerClientImpl::OnVoidMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void SetArcCpuRestriction(
      login_manager::ContainerCpuRestrictionState restriction_state,
      VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(
        login_manager::kSessionManagerInterface,
        login_manager::kSessionManagerSetArcCpuRestriction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint32(restriction_state);
    session_manager_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&SessionManagerClientImpl::OnVoidMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void EmitArcBooted(const cryptohome::Identification& cryptohome_id,
                     VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(login_manager::kSessionManagerInterface,
                                 login_manager::kSessionManagerEmitArcBooted);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(cryptohome_id.id());
    session_manager_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&SessionManagerClientImpl::OnVoidMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetArcStartTime(DBusMethodCallback<base::TimeTicks> callback) override {
    dbus::MethodCall method_call(
        login_manager::kSessionManagerInterface,
        login_manager::kSessionManagerGetArcStartTimeTicks);

    session_manager_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&SessionManagerClientImpl::OnGetArcStartTime,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void RemoveArcData(const cryptohome::Identification& cryptohome_id,
                     VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(login_manager::kSessionManagerInterface,
                                 login_manager::kSessionManagerRemoveArcData);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(cryptohome_id.id());
    session_manager_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&SessionManagerClientImpl::OnVoidMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

 protected:
  void Init(dbus::Bus* bus) override {
    session_manager_proxy_ = bus->GetObjectProxy(
        login_manager::kSessionManagerServiceName,
        dbus::ObjectPath(login_manager::kSessionManagerServicePath));
    blocking_method_caller_.reset(
        new BlockingMethodCaller(bus, session_manager_proxy_));

    // Signals emitted on the session manager's interface.
    session_manager_proxy_->ConnectToSignal(
        login_manager::kSessionManagerInterface,
        login_manager::kOwnerKeySetSignal,
        base::Bind(&SessionManagerClientImpl::OwnerKeySetReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&SessionManagerClientImpl::SignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));
    session_manager_proxy_->ConnectToSignal(
        login_manager::kSessionManagerInterface,
        login_manager::kPropertyChangeCompleteSignal,
        base::Bind(&SessionManagerClientImpl::PropertyChangeCompleteReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&SessionManagerClientImpl::SignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));
    session_manager_proxy_->ConnectToSignal(
        login_manager::kSessionManagerInterface,
        login_manager::kScreenIsLockedSignal,
        base::Bind(&SessionManagerClientImpl::ScreenIsLockedReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&SessionManagerClientImpl::SignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));
    session_manager_proxy_->ConnectToSignal(
        login_manager::kSessionManagerInterface,
        login_manager::kScreenIsUnlockedSignal,
        base::Bind(&SessionManagerClientImpl::ScreenIsUnlockedReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&SessionManagerClientImpl::SignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));
    session_manager_proxy_->ConnectToSignal(
        login_manager::kSessionManagerInterface,
        login_manager::kArcInstanceStopped,
        base::Bind(&SessionManagerClientImpl::ArcInstanceStoppedReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&SessionManagerClientImpl::SignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  // Makes a method call to the session manager with no arguments and no
  // response.
  void SimpleMethodCallToSessionManager(const std::string& method_name) {
    dbus::MethodCall method_call(login_manager::kSessionManagerInterface,
                                 method_name);
    session_manager_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        dbus::ObjectProxy::EmptyResponseCallback());
  }

  // Called when the method call without result is completed.
  void OnVoidMethod(VoidDBusMethodCallback callback, dbus::Response* response) {
    std::move(callback).Run(response);
  }

  // Non-blocking call to Session Manager to retrieve policy.
  void CallRetrievePolicy(const PolicyDescriptor& descriptor,
                          RetrievePolicyCallback callback) {
    dbus::MethodCall method_call(
        login_manager::kSessionManagerInterface,
        login_manager::kSessionManagerRetrievePolicyEx);
    dbus::MessageWriter writer(&method_call);
    const std::string descriptor_blob = descriptor.SerializeAsString();
    // static_cast does not work due to signedness.
    writer.AppendArrayOfBytes(
        reinterpret_cast<const uint8_t*>(descriptor_blob.data()),
        descriptor_blob.size());
    session_manager_proxy_->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&SessionManagerClientImpl::OnRetrievePolicy,
                       weak_ptr_factory_.GetWeakPtr(),
                       descriptor.account_type(), std::move(callback)));
  }

  // Blocking call to Session Manager to retrieve policy.
  RetrievePolicyResponseType BlockingRetrievePolicy(
      const PolicyDescriptor& descriptor,
      std::string* policy_out) {
    dbus::MethodCall method_call(
        login_manager::kSessionManagerInterface,
        login_manager::kSessionManagerRetrievePolicyEx);
    dbus::MessageWriter writer(&method_call);
    const std::string descriptor_blob = descriptor.SerializeAsString();
    // static_cast does not work due to signedness.
    writer.AppendArrayOfBytes(
        reinterpret_cast<const uint8_t*>(descriptor_blob.data()),
        descriptor_blob.size());
    dbus::ScopedDBusError error;
    std::unique_ptr<dbus::Response> response =
        blocking_method_caller_->CallMethodAndBlockWithError(&method_call,
                                                             &error);
    RetrievePolicyResponseType result = RetrievePolicyResponseType::SUCCESS;
    if (error.is_set() && error.name()) {
      result = GetPolicyResponseTypeByError(error.name());
    }
    if (result == RetrievePolicyResponseType::SUCCESS) {
      ExtractPolicyResponseString(descriptor.account_type(), response.get(),
                                  policy_out);
    } else {
      *policy_out = "";
    }
    LogPolicyResponseUma(descriptor.account_type(), result);
    return result;
  }

  void CallStorePolicy(const PolicyDescriptor& descriptor,
                       const std::string& policy_blob,
                       VoidDBusMethodCallback callback) {
    dbus::MethodCall method_call(login_manager::kSessionManagerInterface,
                                 login_manager::kSessionManagerStorePolicyEx);
    dbus::MessageWriter writer(&method_call);
    const std::string descriptor_blob = descriptor.SerializeAsString();
    // static_cast does not work due to signedness.
    writer.AppendArrayOfBytes(
        reinterpret_cast<const uint8_t*>(descriptor_blob.data()),
        descriptor_blob.size());
    writer.AppendArrayOfBytes(
        reinterpret_cast<const uint8_t*>(policy_blob.data()),
        policy_blob.size());
    session_manager_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&SessionManagerClientImpl::OnVoidMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // Called when kSessionManagerRetrieveActiveSessions method is complete.
  void OnRetrieveActiveSessions(const std::string& method_name,
                                ActiveSessionsCallback callback,
                                dbus::Response* response) {
    if (!response) {
      std::move(callback).Run(base::nullopt);
      return;
    }

    dbus::MessageReader reader(response);
    dbus::MessageReader array_reader(nullptr);
    if (!reader.PopArray(&array_reader)) {
      LOG(ERROR) << method_name << " response is incorrect: "
                 << response->ToString();
      std::move(callback).Run(base::nullopt);
      return;
    }

    ActiveSessionsMap sessions;
    while (array_reader.HasMoreData()) {
      dbus::MessageReader dict_entry_reader(nullptr);
      std::string key;
      std::string value;
      if (!array_reader.PopDictEntry(&dict_entry_reader) ||
          !dict_entry_reader.PopString(&key) ||
          !dict_entry_reader.PopString(&value)) {
        LOG(ERROR) << method_name
                   << " response is incorrect: " << response->ToString();
      } else {
        sessions[cryptohome::Identification::FromString(key)] = value;
      }
    }
    std::move(callback).Run(std::move(sessions));
  }

  // Reads an array of policy data bytes data as std::string.
  void ExtractPolicyResponseString(
      login_manager::PolicyAccountType account_type,
      dbus::Response* response,
      std::string* extracted) {
    if (!response) {
      LOG(ERROR) << "Failed to call RetrievePolicyEx for account type "
                 << account_type;
      return;
    }
    dbus::MessageReader reader(response);
    const uint8_t* values = nullptr;
    size_t length = 0;
    if (!reader.PopArrayOfBytes(&values, &length)) {
      LOG(ERROR) << "Invalid response: " << response->ToString();
      return;
    }
    // static_cast does not work due to signedness.
    extracted->assign(reinterpret_cast<const char*>(values), length);
  }

  // Called when kSessionManagerRetrievePolicy or
  // kSessionManagerRetrievePolicyForUser method is complete.
  void OnRetrievePolicy(login_manager::PolicyAccountType account_type,
                        RetrievePolicyCallback callback,
                        dbus::Response* response,
                        dbus::ErrorResponse* error) {
    if (!response) {
      RetrievePolicyResponseType response_type =
          GetPolicyResponseTypeByError(error ? error->GetErrorName() : "");
      LogPolicyResponseUma(account_type, response_type);
      std::move(callback).Run(response_type, std::string());
      return;
    }

    dbus::MessageReader reader(response);
    std::string proto_blob;
    ExtractPolicyResponseString(account_type, response, &proto_blob);
    LogPolicyResponseUma(account_type, RetrievePolicyResponseType::SUCCESS);
    std::move(callback).Run(RetrievePolicyResponseType::SUCCESS, proto_blob);
  }

  // Called when the owner key set signal is received.
  void OwnerKeySetReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    std::string result_string;
    if (!reader.PopString(&result_string)) {
      LOG(ERROR) << "Invalid signal: " << signal->ToString();
      return;
    }
    const bool success = base::StartsWith(result_string, "success",
                                          base::CompareCase::INSENSITIVE_ASCII);
    for (auto& observer : observers_)
      observer.OwnerKeySet(success);
  }

  // Called when the property change complete signal is received.
  void PropertyChangeCompleteReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    std::string result_string;
    if (!reader.PopString(&result_string)) {
      LOG(ERROR) << "Invalid signal: " << signal->ToString();
      return;
    }
    const bool success = base::StartsWith(result_string, "success",
                                          base::CompareCase::INSENSITIVE_ASCII);
    for (auto& observer : observers_)
      observer.PropertyChangeComplete(success);
  }

  void ScreenIsLockedReceived(dbus::Signal* signal) {
    screen_is_locked_ = true;
  }

  void ScreenIsUnlockedReceived(dbus::Signal* signal) {
    screen_is_locked_ = false;
  }

  void ArcInstanceStoppedReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    bool clean = false;
    std::string container_instance_id;
    if (!reader.PopBool(&clean) || !reader.PopString(&container_instance_id)) {
      LOG(ERROR) << "Invalid signal: " << signal->ToString();
      return;
    }
    for (auto& observer : observers_)
      observer.ArcInstanceStopped(clean, container_instance_id);
  }

  // Called when the object is connected to the signal.
  void SignalConnected(const std::string& interface_name,
                       const std::string& signal_name,
                       bool success) {
    LOG_IF(ERROR, !success) << "Failed to connect to " << signal_name;
  }

  // Called when kSessionManagerGetServerBackedStateKeys method is complete.
  void OnGetServerBackedStateKeys(StateKeysCallback callback,
                                  dbus::Response* response) {
    std::vector<std::string> state_keys;
    if (response) {
      dbus::MessageReader reader(response);
      dbus::MessageReader array_reader(nullptr);

      if (!reader.PopArray(&array_reader)) {
        LOG(ERROR) << "Bad response: " << response->ToString();
      } else {
        while (array_reader.HasMoreData()) {
          const uint8_t* data = nullptr;
          size_t size = 0;
          if (!array_reader.PopArrayOfBytes(&data, &size)) {
            LOG(ERROR) << "Bad response: " << response->ToString();
            state_keys.clear();
            break;
          }
          state_keys.emplace_back(reinterpret_cast<const char*>(data), size);
        }
      }
    }

    std::move(callback).Run(state_keys);
  }

  void OnGetArcStartTime(DBusMethodCallback<base::TimeTicks> callback,
                         dbus::Response* response) {
    if (!response) {
      std::move(callback).Run(base::nullopt);
      return;
    }

    dbus::MessageReader reader(response);
    int64_t ticks = 0;
    if (!reader.PopInt64(&ticks)) {
      LOG(ERROR) << "Invalid response: " << response->ToString();
      std::move(callback).Run(base::nullopt);
      return;
    }

    std::move(callback).Run(base::TimeTicks::FromInternalValue(ticks));
  }

  void OnStartArcInstance(StartArcInstanceCallback callback,
                          dbus::Response* response,
                          dbus::ErrorResponse* error) {
    if (!response) {
      LOG(ERROR) << "Failed to call StartArcInstance: "
                 << (error ? error->ToString() : "(null)");
      std::move(callback).Run(
          error && error->GetErrorName() ==
                       login_manager::dbus_error::kLowFreeDisk
              ? StartArcInstanceResult::LOW_FREE_DISK_SPACE
              : StartArcInstanceResult::UNKNOWN_ERROR,
          std::string(), base::ScopedFD());
      return;
    }

    dbus::MessageReader reader(response);
    std::string container_instance_id;
    base::ScopedFD server_socket;
    if (!reader.PopString(&container_instance_id) ||
        !reader.PopFileDescriptor(&server_socket)) {
      LOG(ERROR) << "Invalid response: " << response->ToString();
      std::move(callback).Run(StartArcInstanceResult::UNKNOWN_ERROR,
                              std::string(), base::ScopedFD());
      return;
    }

    std::move(callback).Run(StartArcInstanceResult::SUCCESS,
                            container_instance_id, std::move(server_socket));
  }

  dbus::ObjectProxy* session_manager_proxy_ = nullptr;
  std::unique_ptr<BlockingMethodCaller> blocking_method_caller_;
  base::ObserverList<Observer> observers_;

  // Most recent screen-lock state received from session_manager.
  bool screen_is_locked_ = false;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<SessionManagerClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SessionManagerClientImpl);
};

// The SessionManagerClient implementation used on Linux desktop,
// which does nothing.
class SessionManagerClientStubImpl : public SessionManagerClient {
 public:
  SessionManagerClientStubImpl() = default;
  ~SessionManagerClientStubImpl() override = default;

  // SessionManagerClient overrides:
  void Init(dbus::Bus* bus) override {}
  void SetStubDelegate(StubDelegate* delegate) override {
    delegate_ = delegate;
  }
  void AddObserver(Observer* observer) override {
    observers_.AddObserver(observer);
  }
  void RemoveObserver(Observer* observer) override {
    observers_.RemoveObserver(observer);
  }
  bool HasObserver(const Observer* observer) const override {
    return observers_.HasObserver(observer);
  }
  bool IsScreenLocked() const override { return screen_is_locked_; }
  void EmitLoginPromptVisible() override {}
  void RestartJob(int socket_fd,
                  const std::vector<std::string>& argv,
                  VoidDBusMethodCallback callback) override {}
  void StartSession(const cryptohome::Identification& cryptohome_id) override {}
  void StopSession() override {}
  void NotifySupervisedUserCreationStarted() override {}
  void NotifySupervisedUserCreationFinished() override {}
  void StartDeviceWipe() override {}
  void StartTPMFirmwareUpdate(const std::string& update_mode) override {}
  void RequestLockScreen() override {
    if (delegate_)
      delegate_->LockScreenForStub();
  }
  void NotifyLockScreenShown() override { screen_is_locked_ = true; }
  void NotifyLockScreenDismissed() override { screen_is_locked_ = false; }
  void RetrieveActiveSessions(ActiveSessionsCallback callback) override {}
  void RetrieveDevicePolicy(RetrievePolicyCallback callback) override {
    base::FilePath owner_key_path;
    if (!PathService::Get(chromeos::FILE_OWNER_KEY, &owner_key_path)) {
      std::move(callback).Run(RetrievePolicyResponseType::SUCCESS,
                              std::string());
      return;
    }
    base::FilePath device_policy_path =
        owner_key_path.DirName().AppendASCII(kStubDevicePolicyFile);
    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE,
        {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(&GetFileContent, device_policy_path),
        base::BindOnce(&NotifyOnRetrievePolicySuccess, std::move(callback)));
  }
  RetrievePolicyResponseType BlockingRetrieveDevicePolicy(
      std::string* policy_out) override {
    base::FilePath owner_key_path;
    if (!PathService::Get(chromeos::FILE_OWNER_KEY, &owner_key_path)) {
      *policy_out = "";
      return RetrievePolicyResponseType::SUCCESS;
    }
    base::FilePath device_policy_path =
        owner_key_path.DirName().AppendASCII(kStubDevicePolicyFile);
    *policy_out = GetFileContent(device_policy_path);
    return RetrievePolicyResponseType::SUCCESS;
  }
  void RetrievePolicyForUser(const cryptohome::Identification& cryptohome_id,
                             RetrievePolicyCallback callback) override {
    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE,
        {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(&GetFileContent,
                       GetUserFilePath(cryptohome_id, kStubPolicyFile)),
        base::BindOnce(&NotifyOnRetrievePolicySuccess, std::move(callback)));
  }
  RetrievePolicyResponseType BlockingRetrievePolicyForUser(
      const cryptohome::Identification& cryptohome_id,
      std::string* policy_out) override {
    *policy_out =
        GetFileContent(GetUserFilePath(cryptohome_id, kStubPolicyFile));
    return RetrievePolicyResponseType::SUCCESS;
  }
  void RetrievePolicyForUserWithoutSession(
      const cryptohome::Identification& cryptohome_id,
      RetrievePolicyCallback callback) override {
    RetrievePolicyForUser(cryptohome_id, std::move(callback));
  }
  void RetrieveDeviceLocalAccountPolicy(
      const std::string& account_id,
      RetrievePolicyCallback callback) override {
    RetrievePolicyForUser(cryptohome::Identification::FromString(account_id),
                          std::move(callback));
  }
  RetrievePolicyResponseType BlockingRetrieveDeviceLocalAccountPolicy(
      const std::string& account_id,
      std::string* policy_out) override {
    return BlockingRetrievePolicyForUser(
        cryptohome::Identification::FromString(account_id), policy_out);
  }
  void StoreDevicePolicy(const std::string& policy_blob,
                         VoidDBusMethodCallback callback) override {
    enterprise_management::PolicyFetchResponse response;
    base::FilePath owner_key_path;
    if (!response.ParseFromString(policy_blob) ||
        !PathService::Get(chromeos::FILE_OWNER_KEY, &owner_key_path)) {
      std::move(callback).Run(false);
      return;
    }

    if (response.has_new_public_key()) {
      base::PostTaskWithTraits(
          FROM_HERE,
          {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
          base::BindOnce(&StoreFile, owner_key_path,
                         response.new_public_key()));
    }

    // Chrome will attempt to retrieve the device policy right after storing
    // during enrollment, so make sure it's written before signaling
    // completion.
    // Note also that the owner key will be written before the device policy,
    // if it was present in the blob.
    base::FilePath device_policy_path =
        owner_key_path.DirName().AppendASCII(kStubDevicePolicyFile);
    base::PostTaskWithTraitsAndReply(
        FROM_HERE,
        {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(&StoreFile, device_policy_path, policy_blob),
        base::BindOnce(std::move(callback), true));
  }
  void StorePolicyForUser(const cryptohome::Identification& cryptohome_id,
                          const std::string& policy_blob,
                          VoidDBusMethodCallback callback) override {
    // The session manager writes the user policy key to a well-known
    // location. Do the same with the stub impl, so that user policy works and
    // can be tested on desktop builds.
    enterprise_management::PolicyFetchResponse response;
    if (!response.ParseFromString(policy_blob)) {
      std::move(callback).Run(false);
      return;
    }

    if (response.has_new_public_key()) {
      base::FilePath key_path = GetUserFilePath(cryptohome_id, "policy.pub");
      base::PostTaskWithTraits(
          FROM_HERE,
          {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
          base::BindOnce(&StoreFile, key_path, response.new_public_key()));
    }

    // This file isn't read directly by Chrome, but is used by this class to
    // reload the user policy across restarts.
    base::FilePath stub_policy_path =
        GetUserFilePath(cryptohome_id, kStubPolicyFile);
    base::PostTaskWithTraitsAndReply(
        FROM_HERE,
        {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(&StoreFile, stub_policy_path, policy_blob),
        base::BindOnce(std::move(callback), true));
  }
  void StoreDeviceLocalAccountPolicy(const std::string& account_id,
                                     const std::string& policy_blob,
                                     VoidDBusMethodCallback callback) override {
    StorePolicyForUser(cryptohome::Identification::FromString(account_id),
                       policy_blob, std::move(callback));
  }

  bool SupportsRestartToApplyUserFlags() const override { return false; }

  void SetFlagsForUser(const cryptohome::Identification& cryptohome_id,
                       const std::vector<std::string>& flags) override {}

  void GetServerBackedStateKeys(StateKeysCallback callback) override {
    base::FilePath owner_key_path;
    CHECK(PathService::Get(chromeos::FILE_OWNER_KEY, &owner_key_path));
    const base::FilePath state_keys_path =
        owner_key_path.DirName().AppendASCII(kStubStateKeysFile);
    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE,
        {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(&ReadCreateStateKeysStub, state_keys_path),
        std::move(callback));
  }

  void StartArcInstance(ArcStartupMode startup_mode,
                        const cryptohome::Identification& cryptohome_id,
                        bool disable_boot_completed_broadcast,
                        bool enable_vendor_privileged,
                        bool native_bridge_experiment,
                        StartArcInstanceCallback callback) override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  StartArcInstanceResult::UNKNOWN_ERROR,
                                  std::string(), base::ScopedFD()));
  }

  void SetArcCpuRestriction(
      login_manager::ContainerCpuRestrictionState restriction_state,
      VoidDBusMethodCallback callback) override {
    std::move(callback).Run(false);
  }

  void EmitArcBooted(const cryptohome::Identification& cryptohome_id,
                     VoidDBusMethodCallback callback) override {
    std::move(callback).Run(false);
  }

  void StopArcInstance(VoidDBusMethodCallback callback) override {
    std::move(callback).Run(false);
  }

  void GetArcStartTime(DBusMethodCallback<base::TimeTicks> callback) override {
    std::move(callback).Run(base::nullopt);
  }

  void RemoveArcData(const cryptohome::Identification& cryptohome_id,
                     VoidDBusMethodCallback callback) override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
  }

 private:
  StubDelegate* delegate_ = nullptr;  // Weak pointer; may be nullptr.
  base::ObserverList<Observer> observers_;
  std::string device_policy_;
  bool screen_is_locked_ = false;

  DISALLOW_COPY_AND_ASSIGN(SessionManagerClientStubImpl);
};

SessionManagerClient::SessionManagerClient() = default;

SessionManagerClient::~SessionManagerClient() = default;

SessionManagerClient* SessionManagerClient::Create(
    DBusClientImplementationType type) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION)
    return new SessionManagerClientImpl();
  DCHECK_EQ(FAKE_DBUS_CLIENT_IMPLEMENTATION, type);
  return new SessionManagerClientStubImpl();
}

}  // namespace chromeos
