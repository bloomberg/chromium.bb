// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/bluetooth_manager_client.h"

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/system/runtime_environment.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

// The BluetoothManagerClient implementation used in production.
class BluetoothManagerClientImpl : public BluetoothManagerClient {
 public:
  explicit BluetoothManagerClientImpl(dbus::Bus* bus)
      : weak_ptr_factory_(this),
        object_proxy_(NULL) {
    DVLOG(1) << "Creating BluetoothManagerClientImpl";

    DCHECK(bus);

    object_proxy_ = bus->GetObjectProxy(
        bluetooth_manager::kBluetoothManagerServiceName,
        dbus::ObjectPath(bluetooth_manager::kBluetoothManagerServicePath));

    object_proxy_->ConnectToSignal(
        bluetooth_manager::kBluetoothManagerInterface,
        bluetooth_manager::kAdapterAddedSignal,
        base::Bind(&BluetoothManagerClientImpl::AdapterAddedReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&BluetoothManagerClientImpl::AdapterAddedConnected,
                   weak_ptr_factory_.GetWeakPtr()));

    object_proxy_->ConnectToSignal(
        bluetooth_manager::kBluetoothManagerInterface,
        bluetooth_manager::kAdapterRemovedSignal,
        base::Bind(&BluetoothManagerClientImpl::AdapterRemovedReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&BluetoothManagerClientImpl::AdapterRemovedConnected,
                   weak_ptr_factory_.GetWeakPtr()));

    object_proxy_->ConnectToSignal(
        bluetooth_manager::kBluetoothManagerInterface,
        bluetooth_manager::kDefaultAdapterChangedSignal,
        base::Bind(&BluetoothManagerClientImpl::DefaultAdapterChangedReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&BluetoothManagerClientImpl::DefaultAdapterChangedConnected,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  virtual ~BluetoothManagerClientImpl() {
  }

  // BluetoothManagerClient override.
  virtual void AddObserver(Observer* observer) OVERRIDE {
    DCHECK(observer);
    observers_.AddObserver(observer);
  }

  // BluetoothManagerClient override.
  virtual void RemoveObserver(Observer* observer) OVERRIDE {
    DCHECK(observer);
    observers_.RemoveObserver(observer);
  }

  // BluetoothManagerClient override.
  virtual void DefaultAdapter(const DefaultAdapterCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(
      bluetooth_manager::kBluetoothManagerInterface,
      bluetooth_manager::kDefaultAdapter);

    DCHECK(object_proxy_);
    object_proxy_->CallMethod(
      &method_call,
      dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::Bind(&BluetoothManagerClientImpl::OnDefaultAdapter,
                 weak_ptr_factory_.GetWeakPtr(), callback));
  }

 private:
  // Called by dbus:: when an AdapterAdded signal is received.
  void AdapterAddedReceived(dbus::Signal* signal) {
    DCHECK(signal);
    dbus::MessageReader reader(signal);
    dbus::ObjectPath object_path;
    if (!reader.PopObjectPath(&object_path)) {
      LOG(ERROR) << "AdapterAdded signal has incorrect parameters: "
                 << signal->ToString();
      return;
    }

    DVLOG(1) << "Adapter added: " << object_path.value();
    FOR_EACH_OBSERVER(Observer, observers_, AdapterAdded(object_path));
  }

  // Called by dbus:: when the AdapterAdded signal is initially connected.
  void AdapterAddedConnected(const std::string& interface_name,
                             const std::string& signal_name,
                             bool success) {
    LOG_IF(WARNING, !success) << "Failed to connect to AdapterAdded signal.";
  }

  // Called by dbus:: when an AdapterRemoved signal is received.
  void AdapterRemovedReceived(dbus::Signal* signal) {
    DCHECK(signal);
    dbus::MessageReader reader(signal);
    dbus::ObjectPath object_path;
    if (!reader.PopObjectPath(&object_path)) {
      LOG(ERROR) << "AdapterRemoved signal has incorrect parameters: "
                 << signal->ToString();
      return;
    }

    DVLOG(1) << "Adapter removed: " << object_path.value();
    FOR_EACH_OBSERVER(Observer, observers_, AdapterRemoved(object_path));
  }

  // Called by dbus:: when the AdapterRemoved signal is initially connected.
  void AdapterRemovedConnected(const std::string& interface_name,
                               const std::string& signal_name,
                               bool success) {
    LOG_IF(WARNING, !success) << "Failed to connect to AdapterRemoved signal.";
  }

  // Called by dbus:: when a DefaultAdapterChanged signal is received.
  void DefaultAdapterChangedReceived(dbus::Signal* signal) {
    DCHECK(signal);
    dbus::MessageReader reader(signal);
    dbus::ObjectPath object_path;
    if (!reader.PopObjectPath(&object_path)) {
      LOG(ERROR) << "DefaultAdapterChanged signal has incorrect parameters: "
                 << signal->ToString();
      return;
    }

    DVLOG(1) << "Default adapter changed: " << object_path.value();
    FOR_EACH_OBSERVER(Observer, observers_, DefaultAdapterChanged(object_path));
  }

  // Called by dbus:: when the DefaultAdapterChanged signal is initially
  // connected.
  void DefaultAdapterChangedConnected(const std::string& interface_name,
                                      const std::string& signal_name,
                                      bool success) {
    LOG_IF(WARNING, !success)
        << "Failed to connect to DefaultAdapterChanged signal.";
  }

  // Called when a response for DefaultAdapter() is received.
  void OnDefaultAdapter(const DefaultAdapterCallback& callback,
                        dbus::Response* response) {
    // Parse response.
    bool success = false;
    dbus::ObjectPath object_path;
    if (response != NULL) {
      dbus::MessageReader reader(response);
      if (!reader.PopObjectPath(&object_path)) {
        LOG(ERROR) << "DefaultAdapter response has incorrect parameters: "
                   << response->ToString();
      } else {
        success = true;
      }
    } else {
      LOG(ERROR) << "Failed to get default adapter.";
    }

    // Notify client.
    callback.Run(object_path, success);
  }

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  base::WeakPtrFactory<BluetoothManagerClientImpl> weak_ptr_factory_;

  // D-Bus proxy for BlueZ Manager interface.
  dbus::ObjectProxy* object_proxy_;

  // List of observers interested in event notifications from us.
  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothManagerClientImpl);
};

// The BluetoothManagerClient implementation used on Linux desktop, which does
// nothing.
class BluetoothManagerClientStubImpl : public BluetoothManagerClient {
 public:
  // BluetoothManagerClient override.
  virtual void AddObserver(Observer* observer) OVERRIDE {
  }

  // BluetoothManagerClient override.
  virtual void RemoveObserver(Observer* observer) OVERRIDE {
  }

  // BluetoothManagerClient override.
  virtual void DefaultAdapter(const DefaultAdapterCallback& callback) OVERRIDE {
    VLOG(1) << "Requested default adapter.";
    callback.Run(dbus::ObjectPath(), false);
  }
};

BluetoothManagerClient::BluetoothManagerClient() {
}

BluetoothManagerClient::~BluetoothManagerClient() {
}

BluetoothManagerClient* BluetoothManagerClient::Create(dbus::Bus* bus) {
  if (system::runtime_environment::IsRunningOnChromeOS()) {
    return new BluetoothManagerClientImpl(bus);
  } else {
    return new BluetoothManagerClientStubImpl();
  }
}

}  // namespace chromeos
