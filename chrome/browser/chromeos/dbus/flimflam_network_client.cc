// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/flimflam_network_client.h"

#include "base/bind.h"
#include "base/chromeos/chromeos_version.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "dbus/values_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// The FlimflamNetworkClient implementation.
class FlimflamNetworkClientImpl : public FlimflamNetworkClient {
 public:
  explicit FlimflamNetworkClientImpl(dbus::Bus* bus)
      : proxy_(bus->GetObjectProxy(
          flimflam::kFlimflamServiceName,
          dbus::ObjectPath(flimflam::kFlimflamServicePath))),
        weak_ptr_factory_(this) {
    // We are not using dbus::PropertySet to monitor PropertyChanged signal
    // because the interface is not "org.freedesktop.DBus.Properties".
    proxy_->ConnectToSignal(
        flimflam::kFlimflamNetworkInterface,
        flimflam::kMonitorPropertyChanged,
        base::Bind(&FlimflamNetworkClientImpl::OnPropertyChanged,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&FlimflamNetworkClientImpl::OnSignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  // FlimflamNetworkClient override.
  virtual void SetPropertyChangedHandler(
      const PropertyChangedHandler& handler) OVERRIDE {
    property_changed_handler_ = handler;
  }

  // FlimflamNetworkClient override.
  virtual void ResetPropertyChangedHandler() OVERRIDE {
    property_changed_handler_.Reset();
  }

  // FlimflamNetworkClient override.
  virtual void GetProperties(const DictionaryValueCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamNetworkInterface,
                                 flimflam::kGetPropertiesFunction);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&FlimflamNetworkClientImpl::OnDictionaryValueMethod,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback));
  }

 private:
  // Handles the result of signal connection setup.
  void OnSignalConnected(const std::string& interface,
                         const std::string& signal,
                         bool success) {
    LOG_IF(ERROR, !success) << "Connect to " << interface << " "
                            << signal << " failed.";
  }

  // Handles PropertyChanged signal.
  void OnPropertyChanged(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    std::string name;
    scoped_ptr<base::Value> value;
    const bool name_popped = reader.PopString(&name);
    value.reset(dbus::PopDataAsValue(&reader));
    if (!name_popped || !value.get()) {
      return;
    }
    if (!property_changed_handler_.is_null())
      property_changed_handler_.Run(name, *value);
  }

  // Handles responses for methods with DictionaryValue results.
  void OnDictionaryValueMethod(const DictionaryValueCallback& callback,
                               dbus::Response* response) {
    if (!response) {
      base::DictionaryValue result;
      callback.Run(FAILURE, result);
      return;
    }
    dbus::MessageReader reader(response);
    scoped_ptr<base::Value> value(dbus::PopDataAsValue(&reader));
    base::DictionaryValue* result = NULL;
    if (!value.get() || !value->GetAsDictionary(&result)) {
      base::DictionaryValue result;
      callback.Run(FAILURE, result);
      return;
    }
    callback.Run(SUCCESS, *result);
  }

  dbus::ObjectProxy* proxy_;
  base::WeakPtrFactory<FlimflamNetworkClientImpl> weak_ptr_factory_;
  PropertyChangedHandler property_changed_handler_;

  DISALLOW_COPY_AND_ASSIGN(FlimflamNetworkClientImpl);
};

// A stub implementation of FlimflamNetworkClient.
class FlimflamNetworkClientStubImpl : public FlimflamNetworkClient {
 public:
  FlimflamNetworkClientStubImpl() {}

  virtual ~FlimflamNetworkClientStubImpl() {}

  // FlimflamNetworkClient override.
  virtual void SetPropertyChangedHandler(
      const PropertyChangedHandler& handler) OVERRIDE {}

  // FlimflamNetworkClient override.
  virtual void ResetPropertyChangedHandler() OVERRIDE {}

  // FlimflamNetworkClient override.
  virtual void GetProperties(const DictionaryValueCallback& callback) OVERRIDE {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FlimflamNetworkClientStubImpl);
};

}  // namespace

FlimflamNetworkClient::FlimflamNetworkClient() {}

FlimflamNetworkClient::~FlimflamNetworkClient() {}

// static
FlimflamNetworkClient* FlimflamNetworkClient::Create(dbus::Bus* bus) {
  if (base::chromeos::IsRunningOnChromeOS())
    return new FlimflamNetworkClientImpl(bus);
  else
    return new FlimflamNetworkClientStubImpl();
}

}  // namespace chromeos
