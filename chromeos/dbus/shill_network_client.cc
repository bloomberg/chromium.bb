// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/shill_network_client.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chromeos/dbus/shill_property_changed_observer.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "dbus/values_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// The ShillNetworkClient implementation.
class ShillNetworkClientImpl : public ShillNetworkClient {
 public:
  explicit ShillNetworkClientImpl(dbus::Bus* bus)
      : bus_(bus),
        helpers_deleter_(&helpers_) {
  }

  //////////////////////////////////////
  // ShillNetworkClient overrides.
  virtual void AddPropertyChangedObserver(
      const dbus::ObjectPath& network_path,
      ShillPropertyChangedObserver* observer) OVERRIDE {
    GetHelper(network_path)->AddPropertyChangedObserver(observer);
  }

  virtual void RemovePropertyChangedObserver(
      const dbus::ObjectPath& network_path,
      ShillPropertyChangedObserver* observer) OVERRIDE {
    GetHelper(network_path)->RemovePropertyChangedObserver(observer);
  }

  virtual void GetProperties(const dbus::ObjectPath& network_path,
                             const DictionaryValueCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamNetworkInterface,
                                 flimflam::kGetPropertiesFunction);
    GetHelper(network_path)->CallDictionaryValueMethod(&method_call, callback);
  }

  virtual base::DictionaryValue* CallGetPropertiesAndBlock(
      const dbus::ObjectPath& network_path) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamNetworkInterface,
                                 flimflam::kGetPropertiesFunction);
    return GetHelper(network_path)->CallDictionaryValueMethodAndBlock(
        &method_call);
  }

 private:
  typedef std::map<std::string, ShillClientHelper*> HelperMap;

  // Returns the corresponding ShillClientHelper for the profile.
  ShillClientHelper* GetHelper(const dbus::ObjectPath& network_path) {
    HelperMap::iterator it = helpers_.find(network_path.value());
    if (it != helpers_.end())
      return it->second;

    // There is no helper for the profile, create it.
    dbus::ObjectProxy* object_proxy =
        bus_->GetObjectProxy(flimflam::kFlimflamServiceName, network_path);
    ShillClientHelper* helper = new ShillClientHelper(bus_, object_proxy);
    helper->MonitorPropertyChanged(flimflam::kFlimflamNetworkInterface);
    helpers_.insert(HelperMap::value_type(network_path.value(), helper));
    return helper;
  }

  dbus::Bus* bus_;
  HelperMap helpers_;
  STLValueDeleter<HelperMap> helpers_deleter_;

  DISALLOW_COPY_AND_ASSIGN(ShillNetworkClientImpl);
};

// A stub implementation of ShillNetworkClient.
class ShillNetworkClientStubImpl : public ShillNetworkClient {
 public:
  ShillNetworkClientStubImpl() : weak_ptr_factory_(this) {}

  virtual ~ShillNetworkClientStubImpl() {}

  /////////////////////////////////////
  // ShillNetworkClient overrides.
  virtual void AddPropertyChangedObserver(
      const dbus::ObjectPath& network_path,
      ShillPropertyChangedObserver* observer) OVERRIDE {}

  virtual void RemovePropertyChangedObserver(
      const dbus::ObjectPath& network_path,
      ShillPropertyChangedObserver* observer) OVERRIDE {}

  virtual void GetProperties(const dbus::ObjectPath& network_path,
                             const DictionaryValueCallback& callback) OVERRIDE {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&ShillNetworkClientStubImpl::PassEmptyDictionaryValue,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback));
  }

  virtual base::DictionaryValue* CallGetPropertiesAndBlock(
      const dbus::ObjectPath& network_path) OVERRIDE {
    return new base::DictionaryValue;
  }

 private:
  void PassEmptyDictionaryValue(const DictionaryValueCallback& callback) const {
    base::DictionaryValue dictionary;
    callback.Run(DBUS_METHOD_CALL_SUCCESS, dictionary);
  }

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ShillNetworkClientStubImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ShillNetworkClientStubImpl);
};

}  // namespace

ShillNetworkClient::ShillNetworkClient() {}

ShillNetworkClient::~ShillNetworkClient() {}

// static
ShillNetworkClient* ShillNetworkClient::Create(
    DBusClientImplementationType type,
    dbus::Bus* bus) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION)
    return new ShillNetworkClientImpl(bus);
  DCHECK_EQ(STUB_DBUS_CLIENT_IMPLEMENTATION, type);
  return new ShillNetworkClientStubImpl();
}

}  // namespace chromeos
