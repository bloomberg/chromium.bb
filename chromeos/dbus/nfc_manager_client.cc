// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/nfc_manager_client.h"

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "dbus/bus.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

NfcManagerClient::Properties::Properties(
    dbus::ObjectProxy* object_proxy,
    const PropertyChangedCallback& callback)
    : NfcPropertySet(object_proxy,
                     nfc_manager::kNfcManagerInterface,
                     callback) {
  RegisterProperty(nfc_manager::kAdaptersProperty, &adapters);
}

NfcManagerClient::Properties::~Properties() {
}


// The NfcManagerClient implementation used in production.
class NfcManagerClientImpl : public NfcManagerClient {
 public:
  NfcManagerClientImpl()
      : object_proxy_(NULL),
        weak_ptr_factory_(this) {
  }

  virtual ~NfcManagerClientImpl() {
  }

  // NfcManagerClient override.
  virtual void AddObserver(Observer* observer) OVERRIDE {
    DCHECK(observer);
    observers_.AddObserver(observer);
  }

  // NfcManagerClient override.
  virtual void RemoveObserver(Observer* observer) OVERRIDE {
    DCHECK(observer);
    observers_.RemoveObserver(observer);
  }

  // NfcManagerClient override.
  virtual Properties* GetProperties() OVERRIDE {
    return properties_.get();
  }

 protected:
  // DBusClient override.
  virtual void Init(dbus::Bus* bus) OVERRIDE {
    VLOG(1) << "Creating NfcManagerClientImpl";

    // Create the object proxy.
    object_proxy_ = bus->GetObjectProxy(
        nfc_manager::kNfcManagerServiceName,
        dbus::ObjectPath(nfc_manager::kNfcManagerServicePath));

    // Set up the signal handlers.
    object_proxy_->ConnectToSignal(
        nfc_manager::kNfcManagerInterface,
        nfc_manager::kAdapterAddedSignal,
        base::Bind(&NfcManagerClientImpl::AdapterAddedReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&NfcManagerClientImpl::AdapterAddedConnected,
                   weak_ptr_factory_.GetWeakPtr()));

    object_proxy_->ConnectToSignal(
        nfc_manager::kNfcManagerInterface,
        nfc_manager::kAdapterRemovedSignal,
        base::Bind(&NfcManagerClientImpl::AdapterRemovedReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&NfcManagerClientImpl::AdapterRemovedConnected,
                   weak_ptr_factory_.GetWeakPtr()));

    // Create the properties structure.
    properties_.reset(new Properties(
        object_proxy_,
        base::Bind(&NfcManagerClientImpl::OnPropertyChanged,
                   weak_ptr_factory_.GetWeakPtr())));

    properties_->ConnectSignals();
    properties_->GetAll();
  }

 private:
  // NFC manager signal handlers.
  void OnPropertyChanged(const std::string& property_name) {
    VLOG(1) << "NFC Manager property changed: " << property_name;
    FOR_EACH_OBSERVER(Observer, observers_,
                      ManagerPropertyChanged(property_name));
  }

  // Called by dbus:: when an "AdapterAdded" signal is received..
  void AdapterAddedReceived(dbus::Signal* signal) {
    DCHECK(signal);
    dbus::MessageReader reader(signal);
    dbus::ObjectPath object_path;
    if (!reader.PopObjectPath(&object_path)) {
      LOG(WARNING) << "AdapterAdded signal has incorrect parameters: "
                   << signal->ToString();
      return;
    }
    VLOG(1) << "Adapter added: " << object_path.value();
    FOR_EACH_OBSERVER(Observer, observers_, AdapterAdded(object_path));
  }

  // Called by dbus:: when the "AdapterAdded" signal is initially connected.
  void AdapterAddedConnected(const std::string& interface_name,
                             const std::string& signal_name,
                             bool success) {
    LOG_IF(WARNING, !success) << "Failed to connect to AdapterAdded signal.";
  }

  // Called by dbus:: when an "AdapterRemoved" signal is received..
  void AdapterRemovedReceived(dbus::Signal* signal) {
    DCHECK(signal);
    dbus::MessageReader reader(signal);
    dbus::ObjectPath object_path;
    if (!reader.PopObjectPath(&object_path)) {
      LOG(WARNING) << "AdapterRemoved signal has incorrect parameters: "
                   << signal->ToString();
      return;
    }
    VLOG(1) << "Adapter removed: " << object_path.value();
    FOR_EACH_OBSERVER(Observer, observers_, AdapterRemoved(object_path));
  }

  // Called by dbus:: when the "AdapterAdded" signal is initially connected.
  void AdapterRemovedConnected(const std::string& interface_name,
                               const std::string& signal_name,
                               bool success) {
    LOG_IF(WARNING, !success) << "Failed to connect to AdapterRemoved signal.";
  }

  // D-Bus proxy for neard Manager interface.
  dbus::ObjectProxy* object_proxy_;

  // Properties for neard Manager interface.
  scoped_ptr<Properties> properties_;

  // List of observers interested in event notifications.
  ObserverList<NfcManagerClient::Observer> observers_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<NfcManagerClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NfcManagerClientImpl);
};

NfcManagerClient::NfcManagerClient() {
}

NfcManagerClient::~NfcManagerClient() {
}

// static
NfcManagerClient* NfcManagerClient::Create() {
  return new NfcManagerClientImpl();
}

}  // namespace chromeos
