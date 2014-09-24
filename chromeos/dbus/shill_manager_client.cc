// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/shill_manager_client.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/values.h"
#include "chromeos/dbus/shill_property_changed_observer.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "dbus/values_util.h"
#include "net/base/ip_endpoint.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// The ShillManagerClient implementation.
class ShillManagerClientImpl : public ShillManagerClient {
 public:
  ShillManagerClientImpl() : proxy_(NULL) {}

  ////////////////////////////////////
  // ShillManagerClient overrides.
  virtual void AddPropertyChangedObserver(
      ShillPropertyChangedObserver* observer) OVERRIDE {
    helper_->AddPropertyChangedObserver(observer);
  }

  virtual void RemovePropertyChangedObserver(
      ShillPropertyChangedObserver* observer) OVERRIDE {
    helper_->RemovePropertyChangedObserver(observer);
  }

  virtual void GetProperties(const DictionaryValueCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kGetPropertiesFunction);
    helper_->CallDictionaryValueMethod(&method_call, callback);
  }

  virtual void GetNetworksForGeolocation(
      const DictionaryValueCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kGetNetworksForGeolocation);
    helper_->CallDictionaryValueMethod(&method_call, callback);
  }

  virtual void SetProperty(const std::string& name,
                           const base::Value& value,
                           const base::Closure& callback,
                           const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kSetPropertyFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(name);
    ShillClientHelper::AppendValueDataAsVariant(&writer, value);
    helper_->CallVoidMethodWithErrorCallback(&method_call,
                                            callback,
                                            error_callback);
  }

  virtual void RequestScan(const std::string& type,
                           const base::Closure& callback,
                           const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kRequestScanFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(type);
    helper_->CallVoidMethodWithErrorCallback(&method_call,
                                            callback,
                                            error_callback);
  }

  virtual void EnableTechnology(
      const std::string& type,
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kEnableTechnologyFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(type);
    helper_->CallVoidMethodWithErrorCallback(&method_call,
                                            callback,
                                            error_callback);
  }

  virtual void DisableTechnology(
      const std::string& type,
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kDisableTechnologyFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(type);
    helper_->CallVoidMethodWithErrorCallback(&method_call,
                                            callback,
                                            error_callback);
  }

  virtual void ConfigureService(
      const base::DictionaryValue& properties,
      const ObjectPathCallback& callback,
      const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kConfigureServiceFunction);
    dbus::MessageWriter writer(&method_call);
    ShillClientHelper::AppendServicePropertiesDictionary(&writer, properties);
    helper_->CallObjectPathMethodWithErrorCallback(&method_call,
                                                  callback,
                                                  error_callback);
  }

  virtual void ConfigureServiceForProfile(
      const dbus::ObjectPath& profile_path,
      const base::DictionaryValue& properties,
      const ObjectPathCallback& callback,
      const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kConfigureServiceForProfileFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendObjectPath(dbus::ObjectPath(profile_path));
    ShillClientHelper::AppendServicePropertiesDictionary(&writer, properties);
    helper_->CallObjectPathMethodWithErrorCallback(&method_call,
                                                  callback,
                                                  error_callback);
  }

  virtual void GetService(
      const base::DictionaryValue& properties,
      const ObjectPathCallback& callback,
      const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kGetServiceFunction);
    dbus::MessageWriter writer(&method_call);
    ShillClientHelper::AppendServicePropertiesDictionary(&writer, properties);
    helper_->CallObjectPathMethodWithErrorCallback(&method_call,
                                                  callback,
                                                  error_callback);
  }

  virtual void VerifyDestination(const VerificationProperties& properties,
                                 const BooleanCallback& callback,
                                 const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kVerifyDestinationFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(properties.certificate);
    writer.AppendString(properties.public_key);
    writer.AppendString(properties.nonce);
    writer.AppendString(properties.signed_data);
    writer.AppendString(properties.device_serial);
    writer.AppendString(properties.device_ssid);
    writer.AppendString(properties.device_bssid);
    helper_->CallBooleanMethodWithErrorCallback(
        &method_call, callback, error_callback);
  }

  virtual void VerifyAndEncryptCredentials(
      const VerificationProperties& properties,
      const std::string& service_path,
      const StringCallback& callback,
      const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kVerifyAndEncryptCredentialsFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(properties.certificate);
    writer.AppendString(properties.public_key);
    writer.AppendString(properties.nonce);
    writer.AppendString(properties.signed_data);
    writer.AppendString(properties.device_serial);
    writer.AppendString(properties.device_ssid);
    writer.AppendString(properties.device_bssid);
    writer.AppendObjectPath(dbus::ObjectPath(service_path));
    helper_->CallStringMethodWithErrorCallback(
        &method_call, callback, error_callback);
  }

  virtual void VerifyAndEncryptData(
      const VerificationProperties& properties,
      const std::string& data,
      const StringCallback& callback,
      const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kVerifyAndEncryptDataFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(properties.certificate);
    writer.AppendString(properties.public_key);
    writer.AppendString(properties.nonce);
    writer.AppendString(properties.signed_data);
    writer.AppendString(properties.device_serial);
    writer.AppendString(properties.device_ssid);
    writer.AppendString(properties.device_bssid);
    writer.AppendString(data);
    helper_->CallStringMethodWithErrorCallback(
        &method_call, callback, error_callback);
  }

  virtual void ConnectToBestServices(
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kConnectToBestServicesFunction);
    helper_->CallVoidMethodWithErrorCallback(&method_call,
                                            callback,
                                            error_callback);
  }

  virtual void AddWakeOnPacketConnection(
      const net::IPEndPoint& ip_endpoint,
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE {
    if (ip_endpoint.address().empty()) {
      LOG(ERROR) << "AddWakeOnPacketConnection: null address";
      return;
    }
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kAddWakeOnPacketConnectionFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(net::IPAddressToString(ip_endpoint.address()));
    helper_->CallVoidMethodWithErrorCallback(&method_call,
                                             callback,
                                             error_callback);
  }

  virtual void RemoveWakeOnPacketConnection(
      const net::IPEndPoint& ip_endpoint,
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE {
    if (ip_endpoint.address().empty()) {
      LOG(ERROR) << "RemoveWakeOnPacketConnection: null address";
      return;
    }
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kRemoveWakeOnPacketConnectionFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(net::IPAddressToString(ip_endpoint.address()));
    helper_->CallVoidMethodWithErrorCallback(&method_call,
                                             callback,
                                             error_callback);
  }

  virtual void RemoveAllWakeOnPacketConnections(
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(
        shill::kFlimflamManagerInterface,
        shill::kRemoveAllWakeOnPacketConnectionsFunction);
    helper_->CallVoidMethodWithErrorCallback(&method_call,
                                             callback,
                                             error_callback);
  }

  virtual TestInterface* GetTestInterface() OVERRIDE {
    return NULL;
  }

 protected:
  virtual void Init(dbus::Bus* bus) OVERRIDE {
    proxy_ = bus->GetObjectProxy(shill::kFlimflamServiceName,
                                 dbus::ObjectPath(shill::kFlimflamServicePath));
    helper_.reset(new ShillClientHelper(proxy_));
    helper_->MonitorPropertyChanged(shill::kFlimflamManagerInterface);
  }

 private:
  dbus::ObjectProxy* proxy_;
  scoped_ptr<ShillClientHelper> helper_;

  DISALLOW_COPY_AND_ASSIGN(ShillManagerClientImpl);
};

}  // namespace

ShillManagerClient::ShillManagerClient() {}

ShillManagerClient::~ShillManagerClient() {}

// static
ShillManagerClient* ShillManagerClient::Create() {
  return new ShillManagerClientImpl();
}

// ShillManagerClient::VerificationProperties implementation.
ShillManagerClient::VerificationProperties::VerificationProperties() {
}

ShillManagerClient::VerificationProperties::~VerificationProperties() {
}

}  // namespace chromeos
