// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/session_manager_client.h"

#include <map>

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/threading/worker_pool.h"
#include "chromeos/chromeos_paths.h"
#include "chromeos/dbus/blocking_method_caller.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

// The SessionManagerClient implementation used in production.
class SessionManagerClientImpl : public SessionManagerClient {
 public:
  SessionManagerClientImpl()
      : session_manager_proxy_(NULL),
        weak_ptr_factory_(this) {}

  virtual ~SessionManagerClientImpl() {
  }

  // SessionManagerClient overrides:
  virtual void AddObserver(Observer* observer) OVERRIDE {
    observers_.AddObserver(observer);
  }

  virtual void RemoveObserver(Observer* observer) OVERRIDE {
    observers_.RemoveObserver(observer);
  }

  virtual bool HasObserver(Observer* observer) OVERRIDE {
    return observers_.HasObserver(observer);
  }

  virtual void EmitLoginPromptVisible() OVERRIDE {
    SimpleMethodCallToSessionManager(
        login_manager::kSessionManagerEmitLoginPromptVisible);
    FOR_EACH_OBSERVER(Observer, observers_, EmitLoginPromptVisibleCalled());
  }

  virtual void RestartJob(int pid, const std::string& command_line) OVERRIDE {
    dbus::MethodCall method_call(login_manager::kSessionManagerInterface,
                                 login_manager::kSessionManagerRestartJob);
    dbus::MessageWriter writer(&method_call);
    writer.AppendInt32(pid);
    writer.AppendString(command_line);
    session_manager_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&SessionManagerClientImpl::OnRestartJob,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  virtual void StartSession(const std::string& user_email) OVERRIDE {
    dbus::MethodCall method_call(login_manager::kSessionManagerInterface,
                                 login_manager::kSessionManagerStartSession);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(user_email);
    writer.AppendString("");  // Unique ID is deprecated
    session_manager_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&SessionManagerClientImpl::OnStartSession,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  virtual void StopSession() OVERRIDE {
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

  virtual void StartDeviceWipe() OVERRIDE {
    dbus::MethodCall method_call(login_manager::kSessionManagerInterface,
                                 login_manager::kSessionManagerStartDeviceWipe);
    session_manager_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&SessionManagerClientImpl::OnDeviceWipe,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  virtual void RequestLockScreen() OVERRIDE {
    SimpleMethodCallToSessionManager(login_manager::kSessionManagerLockScreen);
  }

  virtual void NotifyLockScreenShown() OVERRIDE {
    SimpleMethodCallToSessionManager(
        login_manager::kSessionManagerHandleLockScreenShown);
  }

  virtual void NotifyLockScreenDismissed() OVERRIDE {
    SimpleMethodCallToSessionManager(
        login_manager::kSessionManagerHandleLockScreenDismissed);
  }

  virtual void RetrieveActiveSessions(
      const ActiveSessionsCallback& callback) OVERRIDE {
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

  virtual void RetrieveDevicePolicy(
      const RetrievePolicyCallback& callback) OVERRIDE {
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

  virtual void RetrievePolicyForUser(
      const std::string& username,
      const RetrievePolicyCallback& callback) OVERRIDE {
    CallRetrievePolicyByUsername(
        login_manager::kSessionManagerRetrievePolicyForUser,
        username,
        callback);
  }

  virtual std::string BlockingRetrievePolicyForUser(
      const std::string& username) OVERRIDE {
    dbus::MethodCall method_call(
        login_manager::kSessionManagerInterface,
        login_manager::kSessionManagerRetrievePolicyForUser);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(username);
    scoped_ptr<dbus::Response> response =
        blocking_method_caller_->CallMethodAndBlock(&method_call);
    std::string policy;
    ExtractString(login_manager::kSessionManagerRetrievePolicyForUser,
                  response.get(),
                  &policy);
    return policy;
  }

  virtual void RetrieveDeviceLocalAccountPolicy(
      const std::string& account_name,
      const RetrievePolicyCallback& callback) OVERRIDE {
    CallRetrievePolicyByUsername(
        login_manager::kSessionManagerRetrieveDeviceLocalAccountPolicy,
        account_name,
        callback);
  }

  virtual void StoreDevicePolicy(const std::string& policy_blob,
                                 const StorePolicyCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(login_manager::kSessionManagerInterface,
                                 login_manager::kSessionManagerStorePolicy);
    dbus::MessageWriter writer(&method_call);
    // static_cast does not work due to signedness.
    writer.AppendArrayOfBytes(
        reinterpret_cast<const uint8*>(policy_blob.data()), policy_blob.size());
    session_manager_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&SessionManagerClientImpl::OnStorePolicy,
                   weak_ptr_factory_.GetWeakPtr(),
                   login_manager::kSessionManagerStorePolicy,
                   callback));
  }

  virtual void StorePolicyForUser(
      const std::string& username,
      const std::string& policy_blob,
      const std::string& ignored_policy_key,
      const StorePolicyCallback& callback) OVERRIDE {
    CallStorePolicyByUsername(login_manager::kSessionManagerStorePolicyForUser,
                              username,
                              policy_blob,
                              callback);
  }

  virtual void StoreDeviceLocalAccountPolicy(
      const std::string& account_name,
      const std::string& policy_blob,
      const StorePolicyCallback& callback) OVERRIDE {
    CallStorePolicyByUsername(
        login_manager::kSessionManagerStoreDeviceLocalAccountPolicy,
        account_name,
        policy_blob,
        callback);
  }

  virtual void SetFlagsForUser(const std::string& username,
                               const std::vector<std::string>& flags) OVERRIDE {
    dbus::MethodCall method_call(login_manager::kSessionManagerInterface,
                                 login_manager::kSessionManagerSetFlagsForUser);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(username);
    writer.AppendArrayOfStrings(flags);
    session_manager_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        dbus::ObjectProxy::EmptyResponseCallback());
  }

 protected:
  virtual void Init(dbus::Bus* bus) OVERRIDE {
    session_manager_proxy_ = bus->GetObjectProxy(
        login_manager::kSessionManagerServiceName,
        dbus::ObjectPath(login_manager::kSessionManagerServicePath));
    blocking_method_caller_.reset(
        new BlockingMethodCaller(bus, session_manager_proxy_));

    // Signals emitted on Chromium's interface.  Many of these ought to be
    // method calls instead.
    session_manager_proxy_->ConnectToSignal(
        chromium::kChromiumInterface,
        chromium::kLockScreenSignal,
        base::Bind(&SessionManagerClientImpl::ScreenLockReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&SessionManagerClientImpl::SignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));

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
                                    const std::string& username,
                                    const RetrievePolicyCallback& callback) {
    dbus::MethodCall method_call(login_manager::kSessionManagerInterface,
                                 method_name);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(username);
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
                                 const std::string& username,
                                 const std::string& policy_blob,
                                 const StorePolicyCallback& callback) {
    dbus::MethodCall method_call(login_manager::kSessionManagerInterface,
                                 method_name);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(username);
    // static_cast does not work due to signedness.
    writer.AppendArrayOfBytes(
        reinterpret_cast<const uint8*>(policy_blob.data()), policy_blob.size());
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
  void OnRestartJob(dbus::Response* response) {
    LOG_IF(ERROR, !response)
        << "Failed to call "
        << login_manager::kSessionManagerRestartJob;
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
          sessions[key] = value;
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
    const uint8* values = NULL;
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
    const bool success = StartsWithASCII(result_string, "success", false);
    FOR_EACH_OBSERVER(Observer, observers_, OwnerKeySet(success));
  }

  // Called when the property change complete signal is received.
  void PropertyChangeCompleteReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    std::string result_string;
    if (!reader.PopString(&result_string)) {
      LOG(ERROR) << "Invalid signal: " << signal->ToString();
      return;
    }
    const bool success = StartsWithASCII(result_string, "success", false);
    FOR_EACH_OBSERVER(Observer, observers_, PropertyChangeComplete(success));
  }

  void ScreenLockReceived(dbus::Signal* signal) {
    FOR_EACH_OBSERVER(Observer, observers_, LockScreen());
  }

  void ScreenIsLockedReceived(dbus::Signal* signal) {
    FOR_EACH_OBSERVER(Observer, observers_, ScreenIsLocked());
  }

  void ScreenIsUnlockedReceived(dbus::Signal* signal) {
    FOR_EACH_OBSERVER(Observer, observers_, ScreenIsUnlocked());
  }

  // Called when the object is connected to the signal.
  void SignalConnected(const std::string& interface_name,
                       const std::string& signal_name,
                       bool success) {
    LOG_IF(ERROR, !success) << "Failed to connect to " << signal_name;
  }

  dbus::ObjectProxy* session_manager_proxy_;
  scoped_ptr<BlockingMethodCaller> blocking_method_caller_;
  ObserverList<Observer> observers_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<SessionManagerClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SessionManagerClientImpl);
};

// The SessionManagerClient implementation used on Linux desktop,
// which does nothing.
class SessionManagerClientStubImpl : public SessionManagerClient {
 public:
  SessionManagerClientStubImpl() {}
  virtual ~SessionManagerClientStubImpl() {}

  // SessionManagerClient overrides
  virtual void Init(dbus::Bus* bus) OVERRIDE {
    // Make sure that there are no keys left over from a previous browser run.
    base::FilePath user_policy_key_dir;
    if (PathService::Get(chromeos::DIR_USER_POLICY_KEYS,
                         &user_policy_key_dir)) {
      base::WorkerPool::PostTask(
          FROM_HERE,
          base::Bind(base::IgnoreResult(&base::DeleteFile),
                     user_policy_key_dir, true),
          false);
    }
  }

  virtual void AddObserver(Observer* observer) OVERRIDE {
    observers_.AddObserver(observer);
  }
  virtual void RemoveObserver(Observer* observer) OVERRIDE {
    observers_.RemoveObserver(observer);
  }
  virtual bool HasObserver(Observer* observer) OVERRIDE {
    return observers_.HasObserver(observer);
  }
  virtual void EmitLoginPromptVisible() OVERRIDE {}
  virtual void RestartJob(int pid, const std::string& command_line) OVERRIDE {}
  virtual void StartSession(const std::string& user_email) OVERRIDE {}
  virtual void StopSession() OVERRIDE {}
  virtual void StartDeviceWipe() OVERRIDE {}
  virtual void RequestLockScreen() OVERRIDE {
    FOR_EACH_OBSERVER(Observer, observers_, LockScreen());
  }
  virtual void NotifyLockScreenShown() OVERRIDE {
    FOR_EACH_OBSERVER(Observer, observers_, ScreenIsLocked());
  }
  virtual void NotifyLockScreenDismissed() OVERRIDE {
    FOR_EACH_OBSERVER(Observer, observers_, ScreenIsUnlocked());
  }
  virtual void RetrieveActiveSessions(
      const ActiveSessionsCallback& callback) OVERRIDE {}
  virtual void RetrieveDevicePolicy(
      const RetrievePolicyCallback& callback) OVERRIDE {
    callback.Run(device_policy_);
  }
  virtual void RetrievePolicyForUser(
      const std::string& username,
      const RetrievePolicyCallback& callback) OVERRIDE {
    callback.Run(user_policies_[username]);
  }
  virtual std::string BlockingRetrievePolicyForUser(
      const std::string& username) OVERRIDE {
    return user_policies_[username];
  }
  virtual void RetrieveDeviceLocalAccountPolicy(
      const std::string& account_name,
      const RetrievePolicyCallback& callback) OVERRIDE {
    callback.Run(user_policies_[account_name]);
  }
  virtual void StoreDevicePolicy(const std::string& policy_blob,
                                 const StorePolicyCallback& callback) OVERRIDE {
    device_policy_ = policy_blob;
    callback.Run(true);
  }
  virtual void StorePolicyForUser(
      const std::string& username,
      const std::string& policy_blob,
      const std::string& policy_key,
      const StorePolicyCallback& callback) OVERRIDE {
    if (policy_key.empty()) {
      user_policies_[username] = policy_blob;
      callback.Run(true);
      return;
    }
    // The session manager writes the user policy key to a well-known
    // location. Do the same with the stub impl, so that user policy works and
    // can be tested on desktop builds.
    // TODO(joaodasilva): parse the PolicyFetchResponse in |policy_blob| to get
    // the policy key directly, after moving the policy protobufs to a top-level
    // directory. The |policy_key| argument to this method can then be removed.
    // http://crbug.com/240269
    base::FilePath key_path;
    if (!PathService::Get(chromeos::DIR_USER_POLICY_KEYS, &key_path)) {
      callback.Run(false);
      return;
    }
    const std::string sanitized =
        CryptohomeClient::GetStubSanitizedUsername(username);
    key_path = key_path.AppendASCII(sanitized).AppendASCII("policy.pub");
    // Assume that the key write is successful.
    user_policies_[username] = policy_blob;
    base::WorkerPool::PostTaskAndReply(
        FROM_HERE,
        base::Bind(&SessionManagerClientStubImpl::StoreFileInBackground,
                   key_path, policy_key),
        base::Bind(callback, true),
        false);
  }
  virtual void StoreDeviceLocalAccountPolicy(
      const std::string& account_name,
      const std::string& policy_blob,
      const StorePolicyCallback& callback) OVERRIDE {
    user_policies_[account_name] = policy_blob;
    callback.Run(true);
  }
  virtual void SetFlagsForUser(const std::string& username,
                               const std::vector<std::string>& flags) OVERRIDE {
  }

  static void StoreFileInBackground(const base::FilePath& path,
                                    const std::string& data) {
    const int size = static_cast<int>(data.size());
    if (!base::CreateDirectory(path.DirName()) ||
        file_util::WriteFile(path, data.data(), size) != size) {
      LOG(WARNING) << "Failed to write policy key to " << path.value();
    }
  }

 private:
  ObserverList<Observer> observers_;
  std::string device_policy_;
  std::map<std::string, std::string> user_policies_;

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
  DCHECK_EQ(STUB_DBUS_CLIENT_IMPLEMENTATION, type);
  return new SessionManagerClientStubImpl();
}

}  // namespace chromeos
