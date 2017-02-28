// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/session_manager_client.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/chromeos_paths.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/blocking_method_caller.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "crypto/sha2.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// TODO(hidehiko): Share the constant between Chrome and ChromeOS.
constexpr char kArcLowDiskError[] =
    "org.chromium.SessionManagerInterface.LowFreeDisk";

constexpr char kStubPolicyFile[] = "stub_policy";
constexpr char kStubDevicePolicyFile[] = "stub_device_policy";

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

}  // namespace

// The SessionManagerClient implementation used in production.
class SessionManagerClientImpl : public SessionManagerClient {
 public:
  SessionManagerClientImpl()
      : session_manager_proxy_(NULL),
        screen_is_locked_(false),
        weak_ptr_factory_(this) {}

  ~SessionManagerClientImpl() override {}

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
                  const VoidDBusMethodCallback& callback) override {
    dbus::MethodCall method_call(login_manager::kSessionManagerInterface,
                                 login_manager::kSessionManagerRestartJob);
    dbus::MessageWriter writer(&method_call);
    writer.AppendFileDescriptor(socket_fd);
    writer.AppendArrayOfStrings(argv);
    session_manager_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&SessionManagerClientImpl::OnRestartJob,
                   weak_ptr_factory_.GetWeakPtr(), callback));
  }

  void StartSession(const cryptohome::Identification& cryptohome_id) override {
    dbus::MethodCall method_call(login_manager::kSessionManagerInterface,
                                 login_manager::kSessionManagerStartSession);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(cryptohome_id.id());
    writer.AppendString("");  // Unique ID is deprecated
    session_manager_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&SessionManagerClientImpl::OnStartSession,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  void StopSession() override {
    dbus::MethodCall method_call(login_manager::kSessionManagerInterface,
                                 login_manager::kSessionManagerStopSession);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString("");  // Unique ID is deprecated
    session_manager_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&SessionManagerClientImpl::OnStopSession,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  void StartDeviceWipe() override {
    dbus::MethodCall method_call(login_manager::kSessionManagerInterface,
                                 login_manager::kSessionManagerStartDeviceWipe);
    session_manager_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&SessionManagerClientImpl::OnDeviceWipe,
                   weak_ptr_factory_.GetWeakPtr()));
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

  void RetrieveActiveSessions(const ActiveSessionsCallback& callback) override {
    dbus::MethodCall method_call(
        login_manager::kSessionManagerInterface,
        login_manager::kSessionManagerRetrieveActiveSessions);

    session_manager_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&SessionManagerClientImpl::OnRetrieveActiveSessions,
                   weak_ptr_factory_.GetWeakPtr(),
                   login_manager::kSessionManagerRetrieveActiveSessions,
                   callback));
  }

  void RetrieveDevicePolicy(const RetrievePolicyCallback& callback) override {
    dbus::MethodCall method_call(login_manager::kSessionManagerInterface,
                                 login_manager::kSessionManagerRetrievePolicy);
    session_manager_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&SessionManagerClientImpl::OnRetrievePolicy,
                   weak_ptr_factory_.GetWeakPtr(),
                   login_manager::kSessionManagerRetrievePolicy,
                   callback));
  }

  std::string BlockingRetrieveDevicePolicy() override {
    dbus::MethodCall method_call(login_manager::kSessionManagerInterface,
                                 login_manager::kSessionManagerRetrievePolicy);
    std::unique_ptr<dbus::Response> response =
        blocking_method_caller_->CallMethodAndBlock(&method_call);
    std::string policy;
    ExtractString(login_manager::kSessionManagerRetrievePolicy, response.get(),
                  &policy);
    return policy;
  }

  void RetrievePolicyForUser(const cryptohome::Identification& cryptohome_id,
                             const RetrievePolicyCallback& callback) override {
    CallRetrievePolicyByUsername(
        login_manager::kSessionManagerRetrievePolicyForUser, cryptohome_id.id(),
        callback);
  }

  std::string BlockingRetrievePolicyForUser(
      const cryptohome::Identification& cryptohome_id) override {
    dbus::MethodCall method_call(
        login_manager::kSessionManagerInterface,
        login_manager::kSessionManagerRetrievePolicyForUser);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(cryptohome_id.id());
    std::unique_ptr<dbus::Response> response =
        blocking_method_caller_->CallMethodAndBlock(&method_call);
    std::string policy;
    ExtractString(login_manager::kSessionManagerRetrievePolicyForUser,
                  response.get(),
                  &policy);
    return policy;
  }

  void RetrieveDeviceLocalAccountPolicy(
      const std::string& account_name,
      const RetrievePolicyCallback& callback) override {
    CallRetrievePolicyByUsername(
        login_manager::kSessionManagerRetrieveDeviceLocalAccountPolicy,
        account_name,
        callback);
  }

  std::string BlockingRetrieveDeviceLocalAccountPolicy(
      const std::string& account_name) override {
    dbus::MethodCall method_call(
        login_manager::kSessionManagerInterface,
        login_manager::kSessionManagerRetrieveDeviceLocalAccountPolicy);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(account_name);
    std::unique_ptr<dbus::Response> response =
        blocking_method_caller_->CallMethodAndBlock(&method_call);
    std::string policy;
    ExtractString(
        login_manager::kSessionManagerRetrieveDeviceLocalAccountPolicy,
        response.get(), &policy);
    return policy;
  }

  void StoreDevicePolicy(const std::string& policy_blob,
                         const StorePolicyCallback& callback) override {
    dbus::MethodCall method_call(login_manager::kSessionManagerInterface,
                                 login_manager::kSessionManagerStorePolicy);
    dbus::MessageWriter writer(&method_call);
    // static_cast does not work due to signedness.
    writer.AppendArrayOfBytes(
        reinterpret_cast<const uint8_t*>(policy_blob.data()),
        policy_blob.size());
    session_manager_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&SessionManagerClientImpl::OnStorePolicy,
                   weak_ptr_factory_.GetWeakPtr(),
                   login_manager::kSessionManagerStorePolicy,
                   callback));
  }

  void StorePolicyForUser(const cryptohome::Identification& cryptohome_id,
                          const std::string& policy_blob,
                          const StorePolicyCallback& callback) override {
    CallStorePolicyByUsername(login_manager::kSessionManagerStorePolicyForUser,
                              cryptohome_id.id(), policy_blob, callback);
  }

  void StoreDeviceLocalAccountPolicy(
      const std::string& account_name,
      const std::string& policy_blob,
      const StorePolicyCallback& callback) override {
    CallStorePolicyByUsername(
        login_manager::kSessionManagerStoreDeviceLocalAccountPolicy,
        account_name,
        policy_blob,
        callback);
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

  void GetServerBackedStateKeys(const StateKeysCallback& callback) override {
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
        base::Bind(&SessionManagerClientImpl::OnGetServerBackedStateKeys,
                   weak_ptr_factory_.GetWeakPtr(), callback));
  }

  void CheckArcAvailability(const ArcCallback& callback) override {
    dbus::MethodCall method_call(
        login_manager::kSessionManagerInterface,
        login_manager::kSessionManagerCheckArcAvailability);

    session_manager_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&SessionManagerClientImpl::OnCheckArcAvailability,
                   weak_ptr_factory_.GetWeakPtr(), callback));
  }

  void StartArcInstance(const cryptohome::Identification& cryptohome_id,
                        bool disable_boot_completed_broadcast,
                        const StartArcInstanceCallback& callback) override {
    dbus::MethodCall method_call(
        login_manager::kSessionManagerInterface,
        login_manager::kSessionManagerStartArcInstance);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(cryptohome_id.id());
    writer.AppendBool(disable_boot_completed_broadcast);
    session_manager_proxy_->CallMethodWithErrorCallback(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&SessionManagerClientImpl::OnStartArcInstanceSucceeded,
                   weak_ptr_factory_.GetWeakPtr(), callback),
        base::Bind(&SessionManagerClientImpl::OnStartArcInstanceFailed,
                   weak_ptr_factory_.GetWeakPtr(), callback));
  }

  void StopArcInstance(const ArcCallback& callback) override {
    dbus::MethodCall method_call(login_manager::kSessionManagerInterface,
                                 login_manager::kSessionManagerStopArcInstance);
    session_manager_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&SessionManagerClientImpl::OnArcMethod,
                   weak_ptr_factory_.GetWeakPtr(),
                   login_manager::kSessionManagerStopArcInstance, callback));
  }

  void SetArcCpuRestriction(
      login_manager::ContainerCpuRestrictionState restriction_state,
      const ArcCallback& callback) override {
    dbus::MethodCall method_call(
        login_manager::kSessionManagerInterface,
        login_manager::kSessionManagerSetArcCpuRestriction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint32(restriction_state);
    session_manager_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&SessionManagerClientImpl::OnArcMethod,
                   weak_ptr_factory_.GetWeakPtr(),
                   login_manager::kSessionManagerSetArcCpuRestriction,
                   callback));
  }

  void EmitArcBooted(const cryptohome::Identification& cryptohome_id,
                     const ArcCallback& callback) override {
    dbus::MethodCall method_call(login_manager::kSessionManagerInterface,
                                 login_manager::kSessionManagerEmitArcBooted);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(cryptohome_id.id());
    session_manager_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&SessionManagerClientImpl::OnArcMethod,
                   weak_ptr_factory_.GetWeakPtr(),
                   login_manager::kSessionManagerEmitArcBooted, callback));
  }

  void GetArcStartTime(const GetArcStartTimeCallback& callback) override {
    dbus::MethodCall method_call(
        login_manager::kSessionManagerInterface,
        login_manager::kSessionManagerGetArcStartTimeTicks);

    session_manager_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&SessionManagerClientImpl::OnGetArcStartTime,
                   weak_ptr_factory_.GetWeakPtr(), callback));
  }

  void RemoveArcData(const cryptohome::Identification& cryptohome_id,
                     const ArcCallback& callback) override {
    dbus::MethodCall method_call(login_manager::kSessionManagerInterface,
                                 login_manager::kSessionManagerRemoveArcData);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(cryptohome_id.id());
    session_manager_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&SessionManagerClientImpl::OnArcMethod,
                   weak_ptr_factory_.GetWeakPtr(),
                   login_manager::kSessionManagerRemoveArcData, callback));
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

  // Helper for RetrieveDeviceLocalAccountPolicy and RetrievePolicyForUser.
  void CallRetrievePolicyByUsername(const std::string& method_name,
                                    const std::string& account_id,
                                    const RetrievePolicyCallback& callback) {
    dbus::MethodCall method_call(login_manager::kSessionManagerInterface,
                                 method_name);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(account_id);
    session_manager_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(
            &SessionManagerClientImpl::OnRetrievePolicy,
            weak_ptr_factory_.GetWeakPtr(),
            method_name,
            callback));
  }

  void CallStorePolicyByUsername(const std::string& method_name,
                                 const std::string& account_id,
                                 const std::string& policy_blob,
                                 const StorePolicyCallback& callback) {
    dbus::MethodCall method_call(login_manager::kSessionManagerInterface,
                                 method_name);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(account_id);
    // static_cast does not work due to signedness.
    writer.AppendArrayOfBytes(
        reinterpret_cast<const uint8_t*>(policy_blob.data()),
        policy_blob.size());
    session_manager_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(
            &SessionManagerClientImpl::OnStorePolicy,
            weak_ptr_factory_.GetWeakPtr(),
            method_name,
            callback));
  }

  // Called when kSessionManagerRestartJob method is complete.
  void OnRestartJob(const VoidDBusMethodCallback& callback,
                    dbus::Response* response) {
    LOG_IF(ERROR, !response)
        << "Failed to call "
        << login_manager::kSessionManagerRestartJob;
    callback.Run(response ? DBUS_METHOD_CALL_SUCCESS
                          : DBUS_METHOD_CALL_FAILURE);
  }

  // Called when kSessionManagerStartSession method is complete.
  void OnStartSession(dbus::Response* response) {
    LOG_IF(ERROR, !response)
        << "Failed to call "
        << login_manager::kSessionManagerStartSession;
  }

  // Called when kSessionManagerStopSession method is complete.
  void OnStopSession(dbus::Response* response) {
    LOG_IF(ERROR, !response)
        << "Failed to call "
        << login_manager::kSessionManagerStopSession;
  }

  // Called when kSessionManagerStopSession method is complete.
  void OnDeviceWipe(dbus::Response* response) {
    LOG_IF(ERROR, !response)
        << "Failed to call "
        << login_manager::kSessionManagerStartDeviceWipe;
  }

  // Called when kSessionManagerRetrieveActiveSessions method is complete.
  void OnRetrieveActiveSessions(const std::string& method_name,
                                const ActiveSessionsCallback& callback,
                                dbus::Response* response) {
    ActiveSessionsMap sessions;
    bool success = false;
    if (!response) {
      LOG(ERROR) << "Failed to call " << method_name;
      callback.Run(sessions, success);
      return;
    }

    dbus::MessageReader reader(response);
    dbus::MessageReader array_reader(NULL);

    if (!reader.PopArray(&array_reader)) {
      LOG(ERROR) << method_name << " response is incorrect: "
                 << response->ToString();
    } else {
      while (array_reader.HasMoreData()) {
        dbus::MessageReader dict_entry_reader(NULL);
        std::string key;
        std::string value;
        if (!array_reader.PopDictEntry(&dict_entry_reader) ||
            !dict_entry_reader.PopString(&key) ||
            !dict_entry_reader.PopString(&value)) {
          LOG(ERROR) << method_name << " response is incorrect: "
                     << response->ToString();
        } else {
          sessions[cryptohome::Identification::FromString(key)] = value;
        }
      }
      success = true;
    }
    callback.Run(sessions, success);
  }

  void ExtractString(const std::string& method_name,
                     dbus::Response* response,
                     std::string* extracted) {
    if (!response) {
      LOG(ERROR) << "Failed to call " << method_name;
      return;
    }
    dbus::MessageReader reader(response);
    const uint8_t* values = NULL;
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
  void OnRetrievePolicy(const std::string& method_name,
                        const RetrievePolicyCallback& callback,
                        dbus::Response* response) {
    std::string serialized_proto;
    ExtractString(method_name, response, &serialized_proto);
    callback.Run(serialized_proto);
  }

  // Called when kSessionManagerStorePolicy or kSessionManagerStorePolicyForUser
  // method is complete.
  void OnStorePolicy(const std::string& method_name,
                     const StorePolicyCallback& callback,
                     dbus::Response* response) {
    bool success = false;
    if (!response) {
      LOG(ERROR) << "Failed to call " << method_name;
    } else {
      dbus::MessageReader reader(response);
      if (!reader.PopBool(&success))
        LOG(ERROR) << "Invalid response: " << response->ToString();
    }
    callback.Run(success);
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
    for (auto& observer : observers_)
      observer.ScreenIsLocked();
  }

  void ScreenIsUnlockedReceived(dbus::Signal* signal) {
    screen_is_locked_ = false;
    for (auto& observer : observers_)
      observer.ScreenIsUnlocked();
  }

  void ArcInstanceStoppedReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    bool clean = false;
    if (!reader.PopBool(&clean)) {
      LOG(ERROR) << "Invalid signal: " << signal->ToString();
      return;
    }
    for (auto& observer : observers_)
      observer.ArcInstanceStopped(clean);
  }

  // Called when the object is connected to the signal.
  void SignalConnected(const std::string& interface_name,
                       const std::string& signal_name,
                       bool success) {
    LOG_IF(ERROR, !success) << "Failed to connect to " << signal_name;
  }

  // Called when kSessionManagerGetServerBackedStateKeys method is complete.
  void OnGetServerBackedStateKeys(const StateKeysCallback& callback,
                                  dbus::Response* response) {
    std::vector<std::string> state_keys;
    if (!response) {
      LOG(ERROR) << "Failed to call "
                 << login_manager::kSessionManagerGetServerBackedStateKeys;
    } else {
      dbus::MessageReader reader(response);
      dbus::MessageReader array_reader(NULL);

      if (!reader.PopArray(&array_reader)) {
        LOG(ERROR) << "Bad response: " << response->ToString();
      } else {
        while (array_reader.HasMoreData()) {
          const uint8_t* data = NULL;
          size_t size = 0;
          if (!array_reader.PopArrayOfBytes(&data, &size)) {
            LOG(ERROR) << "Bad response: " << response->ToString();
            state_keys.clear();
            break;
          }
          state_keys.push_back(
              std::string(reinterpret_cast<const char*>(data), size));
        }
      }
    }

    if (!callback.is_null())
      callback.Run(state_keys);
  }

  // Called when kSessionManagerCheckArcAvailability method is complete.
  void OnCheckArcAvailability(const ArcCallback& callback,
                              dbus::Response* response) {
    bool available = false;
    if (!response) {
      LOG(ERROR) << "Failed to call "
                 << login_manager::kSessionManagerCheckArcAvailability;
    } else {
      dbus::MessageReader reader(response);
      if (!reader.PopBool(&available))
        LOG(ERROR) << "Invalid response: " << response->ToString();
    }
    if (!callback.is_null())
      callback.Run(available);
  }

  void OnGetArcStartTime(const GetArcStartTimeCallback& callback,
                         dbus::Response* response) {
    bool success = false;
    base::TimeTicks arc_start_time;
    if (!response) {
      LOG(ERROR) << "Failed to call "
                 << login_manager::kSessionManagerGetArcStartTimeTicks;
    } else {
      dbus::MessageReader reader(response);
      int64_t ticks = 0;
      if (reader.PopInt64(&ticks)) {
        success = true;
        arc_start_time = base::TimeTicks::FromInternalValue(ticks);
      } else {
        LOG(ERROR) << "Invalid response: " << response->ToString();
      }
    }
    callback.Run(success, arc_start_time);
  }

  // Called when kSessionManagerStartArcInstance or
  // kSessionManagerStopArcInstance methods complete.
  void OnArcMethod(const std::string& method_name,
                   const ArcCallback& callback,
                   dbus::Response* response) {
    bool success = false;
    if (!response) {
      LOG(ERROR) << "Failed to call " << method_name;
    } else {
      success = true;
    }

    if (!callback.is_null())
      callback.Run(success);
  }

  void OnStartArcInstanceSucceeded(const StartArcInstanceCallback& callback,
                                   dbus::Response* response) {
    if (!callback.is_null())
      callback.Run(StartArcInstanceResult::SUCCESS);
  }

  void OnStartArcInstanceFailed(const StartArcInstanceCallback& callback,
                                dbus::ErrorResponse* response) {
    LOG(ERROR) << "Failed to call StartArcInstance: "
               << (response ? response->ToString() : "(null)");
    if (!callback.is_null())
      callback.Run(response && response->GetErrorName() == kArcLowDiskError
                       ? StartArcInstanceResult::LOW_FREE_DISK_SPACE
                       : StartArcInstanceResult::UNKNOWN_ERROR);
  }

  dbus::ObjectProxy* session_manager_proxy_;
  std::unique_ptr<BlockingMethodCaller> blocking_method_caller_;
  base::ObserverList<Observer> observers_;

  // Most recent screen-lock state received from session_manager.
  bool screen_is_locked_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<SessionManagerClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SessionManagerClientImpl);
};

// The SessionManagerClient implementation used on Linux desktop,
// which does nothing.
class SessionManagerClientStubImpl : public SessionManagerClient {
 public:
  SessionManagerClientStubImpl() : delegate_(NULL), screen_is_locked_(false) {}
  ~SessionManagerClientStubImpl() override {}

  // SessionManagerClient overrides
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
                  const VoidDBusMethodCallback& callback) override {}
  void StartSession(const cryptohome::Identification& cryptohome_id) override {}
  void StopSession() override {}
  void NotifySupervisedUserCreationStarted() override {}
  void NotifySupervisedUserCreationFinished() override {}
  void StartDeviceWipe() override {}
  void RequestLockScreen() override {
    if (delegate_)
      delegate_->LockScreenForStub();
  }
  void NotifyLockScreenShown() override {
    screen_is_locked_ = true;
    for (auto& observer : observers_)
      observer.ScreenIsLocked();
  }
  void NotifyLockScreenDismissed() override {
    screen_is_locked_ = false;
    for (auto& observer : observers_)
      observer.ScreenIsUnlocked();
  }
  void RetrieveActiveSessions(const ActiveSessionsCallback& callback) override {
  }
  void RetrieveDevicePolicy(const RetrievePolicyCallback& callback) override {
    base::FilePath owner_key_path;
    if (!PathService::Get(chromeos::FILE_OWNER_KEY, &owner_key_path)) {
      callback.Run("");
      return;
    }
    base::FilePath device_policy_path =
        owner_key_path.DirName().AppendASCII(kStubDevicePolicyFile);
    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE, base::TaskTraits()
                       .WithShutdownBehavior(
                           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN)
                       .MayBlock(),
        base::Bind(&GetFileContent, device_policy_path), callback);
  }
  std::string BlockingRetrieveDevicePolicy() override {
    base::FilePath owner_key_path;
    if (!PathService::Get(chromeos::FILE_OWNER_KEY, &owner_key_path)) {
      return "";
    }
    base::FilePath device_policy_path =
        owner_key_path.DirName().AppendASCII(kStubDevicePolicyFile);
    return GetFileContent(device_policy_path);
  }
  void RetrievePolicyForUser(const cryptohome::Identification& cryptohome_id,
                             const RetrievePolicyCallback& callback) override {
    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE,
        base::TaskTraits()
            .WithShutdownBehavior(
                base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN)
            .MayBlock(),
        base::Bind(&GetFileContent,
                   GetUserFilePath(cryptohome_id, kStubPolicyFile)),
        callback);
  }
  std::string BlockingRetrievePolicyForUser(
      const cryptohome::Identification& cryptohome_id) override {
    return GetFileContent(GetUserFilePath(cryptohome_id, kStubPolicyFile));
  }
  void RetrieveDeviceLocalAccountPolicy(
      const std::string& account_id,
      const RetrievePolicyCallback& callback) override {
    RetrievePolicyForUser(cryptohome::Identification::FromString(account_id),
                          callback);
  }
  std::string BlockingRetrieveDeviceLocalAccountPolicy(
      const std::string& account_id) override {
    return BlockingRetrievePolicyForUser(
        cryptohome::Identification::FromString(account_id));
  }
  void StoreDevicePolicy(const std::string& policy_blob,
                         const StorePolicyCallback& callback) override {
    enterprise_management::PolicyFetchResponse response;
    base::FilePath owner_key_path;
    if (!response.ParseFromString(policy_blob) ||
        !PathService::Get(chromeos::FILE_OWNER_KEY, &owner_key_path)) {
      callback.Run(false);
      return;
    }

    if (response.has_new_public_key()) {
      base::PostTaskWithTraits(
          FROM_HERE, base::TaskTraits()
                         .WithShutdownBehavior(
                             base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN)
                         .MayBlock(),
          base::Bind(&StoreFile, owner_key_path, response.new_public_key()));
    }

    // Chrome will attempt to retrieve the device policy right after storing
    // during enrollment, so make sure it's written before signaling
    // completion.
    // Note also that the owner key will be written before the device policy,
    // if it was present in the blob.
    base::FilePath device_policy_path =
        owner_key_path.DirName().AppendASCII(kStubDevicePolicyFile);
    base::PostTaskWithTraitsAndReply(
        FROM_HERE, base::TaskTraits()
                       .WithShutdownBehavior(
                           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN)
                       .MayBlock(),
        base::Bind(&StoreFile, device_policy_path, policy_blob),
        base::Bind(callback, true));
  }
  void StorePolicyForUser(const cryptohome::Identification& cryptohome_id,
                          const std::string& policy_blob,
                          const StorePolicyCallback& callback) override {
    // The session manager writes the user policy key to a well-known
    // location. Do the same with the stub impl, so that user policy works and
    // can be tested on desktop builds.
    enterprise_management::PolicyFetchResponse response;
    if (!response.ParseFromString(policy_blob)) {
      callback.Run(false);
      return;
    }

    if (response.has_new_public_key()) {
      base::FilePath key_path = GetUserFilePath(cryptohome_id, "policy.pub");
      base::PostTaskWithTraits(
          FROM_HERE, base::TaskTraits()
                         .WithShutdownBehavior(
                             base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN)
                         .MayBlock(),
          base::Bind(&StoreFile, key_path, response.new_public_key()));
    }

    // This file isn't read directly by Chrome, but is used by this class to
    // reload the user policy across restarts.
    base::FilePath stub_policy_path =
        GetUserFilePath(cryptohome_id, kStubPolicyFile);
    base::PostTaskWithTraitsAndReply(
        FROM_HERE, base::TaskTraits()
                       .WithShutdownBehavior(
                           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN)
                       .MayBlock(),
        base::Bind(&StoreFile, stub_policy_path, policy_blob),
        base::Bind(callback, true));
  }
  void StoreDeviceLocalAccountPolicy(
      const std::string& account_id,
      const std::string& policy_blob,
      const StorePolicyCallback& callback) override {
    StorePolicyForUser(cryptohome::Identification::FromString(account_id),
                       policy_blob, callback);
  }

  bool SupportsRestartToApplyUserFlags() const override { return false; }

  void SetFlagsForUser(const cryptohome::Identification& cryptohome_id,
                       const std::vector<std::string>& flags) override {}

  void GetServerBackedStateKeys(const StateKeysCallback& callback) override {
    std::vector<std::string> state_keys;
    for (int i = 0; i < 5; ++i)
      state_keys.push_back(crypto::SHA256HashString(base::IntToString(i)));

    if (!callback.is_null())
      callback.Run(state_keys);
  }

  void CheckArcAvailability(const ArcCallback& callback) override {
    callback.Run(false);
  }

  void StartArcInstance(const cryptohome::Identification& cryptohome_id,
                        bool disable_boot_completed_broadcast,
                        const StartArcInstanceCallback& callback) override {
    callback.Run(StartArcInstanceResult::UNKNOWN_ERROR);
  }

  void SetArcCpuRestriction(
      login_manager::ContainerCpuRestrictionState restriction_state,
      const ArcCallback& callback) override {
    callback.Run(false);
  }

  void EmitArcBooted(const cryptohome::Identification& cryptohome_id,
                     const ArcCallback& callback) override {
    callback.Run(false);
  }

  void StopArcInstance(const ArcCallback& callback) override {
    callback.Run(false);
  }

  void GetArcStartTime(const GetArcStartTimeCallback& callback) override {
    callback.Run(false, base::TimeTicks::Now());
  }

  void RemoveArcData(const cryptohome::Identification& cryptohome_id,
                     const ArcCallback& callback) override {
    if (callback.is_null())
      return;
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  base::Bind(callback, false));
  }

 private:
  StubDelegate* delegate_;  // Weak pointer; may be NULL.
  base::ObserverList<Observer> observers_;
  std::string device_policy_;
  bool screen_is_locked_;

  DISALLOW_COPY_AND_ASSIGN(SessionManagerClientStubImpl);
};

SessionManagerClient::SessionManagerClient() {
}

SessionManagerClient::~SessionManagerClient() {
}

SessionManagerClient* SessionManagerClient::Create(
    DBusClientImplementationType type) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION)
    return new SessionManagerClientImpl();
  DCHECK_EQ(FAKE_DBUS_CLIENT_IMPLEMENTATION, type);
  return new SessionManagerClientStubImpl();
}

}  // namespace chromeos
