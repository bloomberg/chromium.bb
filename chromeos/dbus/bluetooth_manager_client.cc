// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/bluetooth_manager_client.h"

#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "chromeos/dbus/bluetooth_property.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

BluetoothManagerClient::Properties::Properties(
    dbus::ObjectProxy* object_proxy,
    const PropertyChangedCallback& callback)
    : BluetoothPropertySet(object_proxy,
                           bluetooth_manager::kBluetoothManagerInterface,
                           callback) {
  RegisterProperty(bluetooth_manager::kAdaptersProperty, &adapters);
}

BluetoothManagerClient::Properties::~Properties() {
}


// The BluetoothManagerClient implementation used in production.
class BluetoothManagerClientImpl : public BluetoothManagerClient {
 public:
  explicit BluetoothManagerClientImpl(dbus::Bus* bus)
      : object_proxy_(NULL),
        weak_ptr_factory_(this) {
    DVLOG(1) << "Creating BluetoothManagerClientImpl";

    // Create the object proxy.
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

    // Create the properties structure.
    properties_ = new Properties(
        object_proxy_,
        base::Bind(&BluetoothManagerClientImpl::OnPropertyChanged,
                   weak_ptr_factory_.GetWeakPtr()));

    properties_->ConnectSignals();
    properties_->GetAll();
  }

  virtual ~BluetoothManagerClientImpl() {
    // Clean up the Properties structure.
    delete properties_;
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
  virtual Properties* GetProperties() OVERRIDE {
    return properties_;
  }

  // BluetoothManagerClient override.
  virtual void DefaultAdapter(const AdapterCallback& callback) OVERRIDE {
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

  // BluetoothManagerClient override.
  virtual void FindAdapter(const std::string& address,
                           const AdapterCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(
      bluetooth_manager::kBluetoothManagerInterface,
      bluetooth_manager::kFindAdapter);

    dbus::MessageWriter writer(&method_call);
    writer.AppendString(address);

    DCHECK(object_proxy_);
    object_proxy_->CallMethod(
      &method_call,
      dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::Bind(&BluetoothManagerClientImpl::OnFindAdapter,
                 weak_ptr_factory_.GetWeakPtr(), callback));
  }

 private:
  // Called by BluetoothPropertySet when a property value is changed,
  // either by result of a signal or response to a GetAll() or Get()
  // call. Informs observers.
  void OnPropertyChanged(const std::string& property_name) {
    FOR_EACH_OBSERVER(BluetoothManagerClient::Observer, observers_,
                      ManagerPropertyChanged(property_name));
  }

  // Called by dbus:: when an AdapterAdded signal is received.
  void AdapterAddedReceived(dbus::Signal* signal) {
    DCHECK(signal);
    dbus::MessageReader reader(signal);
    dbus::ObjectPath object_path;
    if (!reader.PopObjectPath(&object_path)) {
      LOG(WARNING) << "AdapterAdded signal has incorrect parameters: "
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
      LOG(WARNING) << "AdapterRemoved signal has incorrect parameters: "
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
      LOG(WARNING) << "DefaultAdapterChanged signal has incorrect parameters: "
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
  void OnDefaultAdapter(const AdapterCallback& callback,
                        dbus::Response* response) {
    // Parse response.
    bool success = false;
    dbus::ObjectPath object_path;
    if (response != NULL) {
      dbus::MessageReader reader(response);
      if (!reader.PopObjectPath(&object_path)) {
        LOG(WARNING) << "DefaultAdapter response has incorrect parameters: "
                     << response->ToString();
      } else {
        success = true;
      }
    } else {
      LOG(WARNING) << "Failed to get default adapter.";
    }

    // Notify client.
    callback.Run(object_path, success);
  }

  // Called when a response for FindAdapter() is received.
  void OnFindAdapter(const AdapterCallback& callback,
                     dbus::Response* response) {
    // Parse response.
    bool success = false;
    dbus::ObjectPath object_path;
    if (response != NULL) {
      dbus::MessageReader reader(response);
      if (!reader.PopObjectPath(&object_path)) {
        LOG(WARNING) << "FindAdapter response has incorrect parameters: "
                     << response->ToString();
      } else {
        success = true;
      }
    } else {
      LOG(WARNING) << "Failed to find adapter.";
    }

    // Notify client.
    callback.Run(object_path, success);
  }

  // D-Bus proxy for BlueZ Manager interface.
  dbus::ObjectProxy* object_proxy_;

  // Properties for BlueZ Manager interface.
  Properties* properties_;

  // List of observers interested in event notifications from us.
  ObserverList<Observer> observers_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothManagerClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothManagerClientImpl);
};

// The BluetoothManagerClient implementation used on Linux desktop, which does
// nothing.
class BluetoothManagerClientStubImpl : public BluetoothManagerClient {
 public:
  struct Properties : public BluetoothManagerClient::Properties {
    explicit Properties(const PropertyChangedCallback& callback)
        : BluetoothManagerClient::Properties(NULL, callback) {
    }

    virtual ~Properties() {
    }

    virtual void Get(dbus::PropertyBase* property,
                     dbus::PropertySet::GetCallback callback) OVERRIDE {
      VLOG(1) << "Get " << property->name();
      callback.Run(false);
    }

    virtual void GetAll() OVERRIDE {
      VLOG(1) << "GetAll";
    }

    virtual void Set(dbus::PropertyBase* property,
                     dbus::PropertySet::SetCallback callback) OVERRIDE {
      VLOG(1) << "Set " << property->name();
      callback.Run(false);
    }
  };

  BluetoothManagerClientStubImpl() {
    properties_.reset(new Properties(base::Bind(
        &BluetoothManagerClientStubImpl::OnPropertyChanged,
        base::Unretained(this))));

    std::vector<dbus::ObjectPath> adapters;
    adapters.push_back(dbus::ObjectPath("/fake/hci0"));
    properties_->adapters.ReplaceValue(adapters);
  }

  // BluetoothManagerClient override.
  virtual void AddObserver(Observer* observer) OVERRIDE {
    observers_.AddObserver(observer);
  }

  // BluetoothManagerClient override.
  virtual void RemoveObserver(Observer* observer) OVERRIDE {
    observers_.RemoveObserver(observer);
  }

  // BluetoothManagerClient override.
  virtual Properties* GetProperties() OVERRIDE {
    VLOG(1) << "GetProperties";
    return properties_.get();
  }

  // BluetoothManagerClient override.
  virtual void DefaultAdapter(const AdapterCallback& callback) OVERRIDE {
    VLOG(1) << "DefaultAdapter.";
    callback.Run(dbus::ObjectPath("/fake/hci0"), true);
  }

  // BluetoothManagerClient override.
  virtual void FindAdapter(const std::string& address,
                           const AdapterCallback& callback) OVERRIDE {
    VLOG(1) << "FindAdapter: " << address;
    if (address == "hci0")
      callback.Run(dbus::ObjectPath("/fake/hci0"), true);
    else
      callback.Run(dbus::ObjectPath(), false);
  }

 private:
  void OnPropertyChanged(const std::string& property_name) {
    FOR_EACH_OBSERVER(BluetoothManagerClient::Observer, observers_,
                      ManagerPropertyChanged(property_name));
  }

  // List of observers interested in event notifications from us.
  ObserverList<Observer> observers_;

  // Static properties we return.
  scoped_ptr<Properties> properties_;
};

BluetoothManagerClient::BluetoothManagerClient() {
}

BluetoothManagerClient::~BluetoothManagerClient() {
}

BluetoothManagerClient* BluetoothManagerClient::Create(
    DBusClientImplementationType type,
    dbus::Bus* bus) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION)
    return new BluetoothManagerClientImpl(bus);
  DCHECK_EQ(STUB_DBUS_CLIENT_IMPLEMENTATION, type);
  return new BluetoothManagerClientStubImpl();
}

}  // namespace chromeos
