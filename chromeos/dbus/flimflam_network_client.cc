// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/flimflam_network_client.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/values.h"
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
      : bus_(bus),
        helpers_deleter_(&helpers_) {
  }

  // FlimflamNetworkClient override.
  virtual void SetPropertyChangedHandler(
      const dbus::ObjectPath& network_path,
      const PropertyChangedHandler& handler) OVERRIDE {
    GetHelper(network_path)->SetPropertyChangedHandler(handler);
  }

  // FlimflamNetworkClient override.
  virtual void ResetPropertyChangedHandler(
      const dbus::ObjectPath& network_path) OVERRIDE {
    GetHelper(network_path)->ResetPropertyChangedHandler();
  }

  // FlimflamNetworkClient override.
  virtual void GetProperties(const dbus::ObjectPath& network_path,
                             const DictionaryValueCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamNetworkInterface,
                                 flimflam::kGetPropertiesFunction);
    GetHelper(network_path)->CallDictionaryValueMethod(&method_call, callback);
  }

  // FlimflamNetworkClient override.
  virtual base::DictionaryValue* CallGetPropertiesAndBlock(
      const dbus::ObjectPath& network_path) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamNetworkInterface,
                                 flimflam::kGetPropertiesFunction);
    return GetHelper(network_path)->CallDictionaryValueMethodAndBlock(
        &method_call);
  }

 private:
  typedef std::map<std::string, FlimflamClientHelper*> HelperMap;

  // Returns the corresponding FlimflamClientHelper for the profile.
  FlimflamClientHelper* GetHelper(const dbus::ObjectPath& network_path) {
    HelperMap::iterator it = helpers_.find(network_path.value());
    if (it != helpers_.end())
      return it->second;

    // There is no helper for the profile, create it.
    dbus::ObjectProxy* object_proxy =
        bus_->GetObjectProxy(flimflam::kFlimflamServiceName, network_path);
    FlimflamClientHelper* helper = new FlimflamClientHelper(bus_, object_proxy);
    helper->MonitorPropertyChanged(flimflam::kFlimflamNetworkInterface);
    helpers_.insert(HelperMap::value_type(network_path.value(), helper));
    return helper;
  }

  dbus::Bus* bus_;
  HelperMap helpers_;
  STLValueDeleter<HelperMap> helpers_deleter_;

  DISALLOW_COPY_AND_ASSIGN(FlimflamNetworkClientImpl);
};

// A stub implementation of FlimflamNetworkClient.
class FlimflamNetworkClientStubImpl : public FlimflamNetworkClient {
 public:
  FlimflamNetworkClientStubImpl() : weak_ptr_factory_(this) {}

  virtual ~FlimflamNetworkClientStubImpl() {}

  // FlimflamNetworkClient override.
  virtual void SetPropertyChangedHandler(
      const dbus::ObjectPath& network_path,
      const PropertyChangedHandler& handler) OVERRIDE {}

  // FlimflamNetworkClient override.
  virtual void ResetPropertyChangedHandler(
      const dbus::ObjectPath& network_path) OVERRIDE {}

  // FlimflamNetworkClient override.
  virtual void GetProperties(const dbus::ObjectPath& network_path,
                             const DictionaryValueCallback& callback) OVERRIDE {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&FlimflamNetworkClientStubImpl::PassEmptyDictionaryValue,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback));
  }

  // FlimflamNetworkClient override.
  virtual base::DictionaryValue* CallGetPropertiesAndBlock(
      const dbus::ObjectPath& network_path) OVERRIDE {
    return new base::DictionaryValue;
  }

 private:
  void PassEmptyDictionaryValue(const DictionaryValueCallback& callback) const {
    base::DictionaryValue dictionary;
    callback.Run(DBUS_METHOD_CALL_SUCCESS, dictionary);
  }

  base::WeakPtrFactory<FlimflamNetworkClientStubImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FlimflamNetworkClientStubImpl);
};

}  // namespace

FlimflamNetworkClient::FlimflamNetworkClient() {}

FlimflamNetworkClient::~FlimflamNetworkClient() {}

// static
FlimflamNetworkClient* FlimflamNetworkClient::Create(
    DBusClientImplementationType type,
    dbus::Bus* bus) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION)
    return new FlimflamNetworkClientImpl(bus);
  DCHECK_EQ(STUB_DBUS_CLIENT_IMPLEMENTATION, type);
  return new FlimflamNetworkClientStubImpl();
}

}  // namespace chromeos
