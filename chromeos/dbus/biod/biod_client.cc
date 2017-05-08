// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/biod/biod_client.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "chromeos/dbus/biod/fake_biod_client.h"
#include "chromeos/dbus/biod/messages.pb.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"

namespace chromeos {

namespace {

// D-Bus response handler for methods that use void callbacks.
void OnVoidResponse(const VoidDBusMethodCallback& callback,
                    dbus::Response* response) {
  callback.Run(response ? DBUS_METHOD_CALL_SUCCESS : DBUS_METHOD_CALL_FAILURE);
}

}  // namespace

// The BiodClient implementation used in production.
class BiodClientImpl : public BiodClient {
 public:
  BiodClientImpl() : weak_ptr_factory_(this) {}

  ~BiodClientImpl() override {}

  // BiodClient overrides:
  void AddObserver(Observer* observer) override {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) override {
    observers_.RemoveObserver(observer);
  }

  bool HasObserver(const Observer* observer) const override {
    return observers_.HasObserver(observer);
  }

  void StartEnrollSession(const std::string& user_id,
                          const std::string& label,
                          const ObjectPathCallback& callback) override {
    // If we are already in enroll session, just return an invalid ObjectPath.
    // The one who initially start the enroll session will have control
    // over the life cycle of the session.
    if (current_enroll_session_path_) {
      callback.Run(dbus::ObjectPath());
      return;
    }

    dbus::MethodCall method_call(
        biod::kBiometricsManagerInterface,
        biod::kBiometricsManagerStartEnrollSessionMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(user_id);
    writer.AppendString(label);

    biod_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&BiodClientImpl::OnStartEnrollSession,
                   weak_ptr_factory_.GetWeakPtr(), callback));
  }

  void GetRecordsForUser(const std::string& user_id,
                         const UserRecordsCallback& callback) override {
    dbus::MethodCall method_call(
        biod::kBiometricsManagerInterface,
        biod::kBiometricsManagerGetRecordsForUserMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(user_id);

    biod_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&BiodClientImpl::OnGetRecordsForUser,
                   weak_ptr_factory_.GetWeakPtr(), callback));
  }

  void DestroyAllRecords(const VoidDBusMethodCallback& callback) override {
    dbus::MethodCall method_call(
        biod::kBiometricsManagerInterface,
        biod::kBiometricsManagerDestroyAllRecordsMethod);

    biod_proxy_->CallMethod(&method_call,
                            dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                            base::Bind(&OnVoidResponse, callback));
  }

  void StartAuthSession(const ObjectPathCallback& callback) override {
    // If we are already in auth session, just return an invalid ObjectPath.
    // The one who initially start the auth session will have control
    // over the life cycle of the session.
    if (current_auth_session_path_) {
      callback.Run(dbus::ObjectPath(std::string()));
      return;
    }

    dbus::MethodCall method_call(
        biod::kBiometricsManagerInterface,
        biod::kBiometricsManagerStartAuthSessionMethod);

    biod_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&BiodClientImpl::OnStartAuthSession,
                   weak_ptr_factory_.GetWeakPtr(), callback));
  }

  void RequestType(const BiometricTypeCallback& callback) override {
    dbus::MethodCall method_call(dbus::kDBusPropertiesInterface,
                                 dbus::kDBusPropertiesGet);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(biod::kBiometricsManagerInterface);
    writer.AppendString(biod::kBiometricsManagerBiometricTypeProperty);

    biod_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&BiodClientImpl::OnRequestType,
                   weak_ptr_factory_.GetWeakPtr(), callback));
  }

  void CancelEnrollSession(const VoidDBusMethodCallback& callback) override {
    if (!current_enroll_session_path_) {
      callback.Run(DBUS_METHOD_CALL_SUCCESS);
      return;
    }
    dbus::MethodCall method_call(biod::kEnrollSessionInterface,
                                 biod::kEnrollSessionCancelMethod);

    dbus::ObjectProxy* enroll_session_proxy = bus_->GetObjectProxy(
        biod::kBiodServiceName, *current_enroll_session_path_);
    enroll_session_proxy->CallMethod(&method_call,
                                     dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                     base::Bind(&OnVoidResponse, callback));
    current_enroll_session_path_.reset();
  }

  void EndAuthSession(const VoidDBusMethodCallback& callback) override {
    if (!current_auth_session_path_) {
      callback.Run(DBUS_METHOD_CALL_SUCCESS);
      return;
    }
    dbus::MethodCall method_call(biod::kAuthSessionInterface,
                                 biod::kAuthSessionEndMethod);

    dbus::ObjectProxy* auth_session_proxy = bus_->GetObjectProxy(
        biod::kBiodServiceName, *current_auth_session_path_);
    auth_session_proxy->CallMethod(&method_call,
                                   dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                   base::Bind(&OnVoidResponse, callback));
    current_auth_session_path_.reset();
  }

  void SetRecordLabel(const dbus::ObjectPath& record_path,
                      const std::string& label,
                      const VoidDBusMethodCallback& callback) override {
    dbus::MethodCall method_call(biod::kRecordInterface,
                                 biod::kRecordSetLabelMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(label);

    dbus::ObjectProxy* record_proxy =
        bus_->GetObjectProxy(biod::kBiodServiceName, record_path);
    record_proxy->CallMethod(&method_call,
                             dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                             base::Bind(&OnVoidResponse, callback));
  }

  void RemoveRecord(const dbus::ObjectPath& record_path,
                    const VoidDBusMethodCallback& callback) override {
    dbus::MethodCall method_call(biod::kRecordInterface,
                                 biod::kRecordRemoveMethod);

    dbus::ObjectProxy* record_proxy =
        bus_->GetObjectProxy(biod::kBiodServiceName, record_path);
    record_proxy->CallMethod(&method_call,
                             dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                             base::Bind(&OnVoidResponse, callback));
  }

  void RequestRecordLabel(const dbus::ObjectPath& record_path,
                          const LabelCallback& callback) override {
    dbus::MethodCall method_call(dbus::kDBusPropertiesInterface,
                                 dbus::kDBusPropertiesGet);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(biod::kRecordInterface);
    writer.AppendString(biod::kRecordLabelProperty);

    dbus::ObjectProxy* record_proxy =
        bus_->GetObjectProxy(biod::kBiodServiceName, record_path);
    record_proxy->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&BiodClientImpl::OnRequestRecordLabel,
                   weak_ptr_factory_.GetWeakPtr(), callback));
  }

 protected:
  void Init(dbus::Bus* bus) override {
    bus_ = bus;

    dbus::ObjectPath fpc_bio_path = dbus::ObjectPath(base::StringPrintf(
        "%s/%s", biod::kBiodServicePath, biod::kFpcBiometricsManagerName));
    biod_proxy_ = bus->GetObjectProxy(biod::kBiodServiceName, fpc_bio_path);

    biod_proxy_->SetNameOwnerChangedCallback(
        base::Bind(&BiodClientImpl::NameOwnerChangedReceived,
                   weak_ptr_factory_.GetWeakPtr()));

    biod_proxy_->ConnectToSignal(
        biod::kBiometricsManagerInterface,
        biod::kBiometricsManagerEnrollScanDoneSignal,
        base::Bind(&BiodClientImpl::EnrollScanDoneReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&BiodClientImpl::OnSignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));

    biod_proxy_->ConnectToSignal(
        biod::kBiometricsManagerInterface,
        biod::kBiometricsManagerAuthScanDoneSignal,
        base::Bind(&BiodClientImpl::AuthScanDoneReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&BiodClientImpl::OnSignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));

    biod_proxy_->ConnectToSignal(
        biod::kBiometricsManagerInterface,
        biod::kBiometricsManagerSessionFailedSignal,
        base::Bind(&BiodClientImpl::SessionFailedReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&BiodClientImpl::OnSignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  void OnStartEnrollSession(const ObjectPathCallback& callback,
                            dbus::Response* response) {
    dbus::ObjectPath result;
    if (response) {
      dbus::MessageReader reader(response);
      if (!reader.PopObjectPath(&result)) {
        LOG(ERROR) << biod::kBiometricsManagerStartEnrollSessionMethod
                   << " had incorrect response.";
      }
    }

    if (result.IsValid())
      current_enroll_session_path_ = base::MakeUnique<dbus::ObjectPath>(result);
    callback.Run(result);
  }

  void OnGetRecordsForUser(const UserRecordsCallback& callback,
                           dbus::Response* response) {
    std::vector<dbus::ObjectPath> result;
    if (response) {
      dbus::MessageReader reader(response);
      if (!reader.PopArrayOfObjectPaths(&result)) {
        LOG(ERROR) << biod::kBiometricsManagerGetRecordsForUserMethod
                   << " had incorrect response.";
      }
    }

    callback.Run(result);
  }

  void OnStartAuthSession(const ObjectPathCallback& callback,
                          dbus::Response* response) {
    dbus::ObjectPath result;
    if (response) {
      dbus::MessageReader reader(response);
      if (!reader.PopObjectPath(&result)) {
        LOG(ERROR) << biod::kBiometricsManagerStartAuthSessionMethod
                   << " had incorrect response.";
      }
    }

    if (result.IsValid())
      current_auth_session_path_ = base::MakeUnique<dbus::ObjectPath>(result);
    callback.Run(result);
  }

  void OnRequestType(const BiometricTypeCallback& callback,
                     dbus::Response* response) {
    uint32_t result =
        static_cast<uint32_t>(biod::BiometricType::BIOMETRIC_TYPE_UNKNOWN);
    if (response) {
      dbus::MessageReader reader(response);
      if (!reader.PopVariantOfUint32(&result)) {
        LOG(ERROR) << biod::kBiometricsManagerBiometricTypeProperty
                   << " had incorrect response.";
      }
    }

    callback.Run(result);
  }

  void OnRequestRecordLabel(const LabelCallback& callback,
                            dbus::Response* response) {
    std::string result;
    if (response) {
      dbus::MessageReader reader(response);
      if (!reader.PopVariantOfString(&result))
        LOG(ERROR) << biod::kRecordLabelProperty << " had incorrect response.";
    }

    callback.Run(result);
  }

  // Called when the biometrics signal is initially connected.
  void OnSignalConnected(const std::string& interface_name,
                         const std::string& signal_name,
                         bool success) {
    LOG_IF(ERROR, !success)
        << "Failed to connect to biometrics signal: " << signal_name;
  }

  void NameOwnerChangedReceived(const std::string& /* old_owner */,
                                const std::string& new_owner) {
    current_enroll_session_path_.reset();
    current_auth_session_path_.reset();

    if (!new_owner.empty()) {
      for (auto& observer : observers_)
        observer.BiodServiceRestarted();
    }
  }

  void EnrollScanDoneReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    biod::EnrollScanDone protobuf;
    if (!reader.PopArrayOfBytesAsProto(&protobuf)) {
      LOG(ERROR) << "Unable to decode protocol buffer from "
                 << biod::kBiometricsManagerEnrollScanDoneSignal << " signal.";
      return;
    }

    int percent_complete =
        protobuf.has_percent_complete() ? protobuf.percent_complete() : -1;

    for (auto& observer : observers_) {
      observer.BiodEnrollScanDoneReceived(protobuf.scan_result(),
                                          protobuf.done(), percent_complete);
    }
  }

  void AuthScanDoneReceived(dbus::Signal* signal) {
    dbus::MessageReader signal_reader(signal);
    dbus::MessageReader array_reader(nullptr);
    uint32_t scan_result;
    AuthScanMatches matches;
    if (!signal_reader.PopUint32(&scan_result) ||
        !signal_reader.PopArray(&array_reader)) {
      LOG(ERROR) << "Error reading signal from biometrics: "
                 << signal->ToString();
      return;
    }

    while (array_reader.HasMoreData()) {
      dbus::MessageReader entry_reader(nullptr);
      std::string user_id;
      std::vector<dbus::ObjectPath> paths;
      if (!array_reader.PopDictEntry(&entry_reader) ||
          !entry_reader.PopString(&user_id) ||
          !entry_reader.PopArrayOfObjectPaths(&paths)) {
        LOG(ERROR) << "Error reading signal from biometrics: "
                   << signal->ToString();
        return;
      }

      matches[user_id] = std::move(paths);
    }

    for (auto& observer : observers_) {
      observer.BiodAuthScanDoneReceived(
          static_cast<biod::ScanResult>(scan_result), matches);
    }
  }

  void SessionFailedReceived(dbus::Signal* signal) {
    for (auto& observer : observers_)
      observer.BiodSessionFailedReceived();
  }

  dbus::Bus* bus_ = nullptr;
  dbus::ObjectProxy* biod_proxy_ = nullptr;
  base::ObserverList<Observer> observers_;
  std::unique_ptr<dbus::ObjectPath> current_enroll_session_path_;
  std::unique_ptr<dbus::ObjectPath> current_auth_session_path_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BiodClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BiodClientImpl);
};

BiodClient::BiodClient() {}

BiodClient::~BiodClient() {}

// static
BiodClient* BiodClient::Create(DBusClientImplementationType type) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION)
    return new BiodClientImpl();
  DCHECK_EQ(FAKE_DBUS_CLIENT_IMPLEMENTATION, type);
  return new FakeBiodClient();
}

}  // namespace chromeos
