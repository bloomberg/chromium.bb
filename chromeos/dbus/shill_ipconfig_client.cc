// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/shill_ipconfig_client.h"

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

// The ShillIPConfigClient implementation.
class ShillIPConfigClientImpl : public ShillIPConfigClient {
 public:
  explicit ShillIPConfigClientImpl(dbus::Bus* bus);

  // ShillIPConfigClient overrides:
  virtual void SetPropertyChangedHandler(
      const dbus::ObjectPath& ipconfig_path,
      const PropertyChangedHandler& handler) OVERRIDE;
  virtual void ResetPropertyChangedHandler(
      const dbus::ObjectPath& ipconfig_path) OVERRIDE;
  virtual void Refresh(const dbus::ObjectPath& ipconfig_path,
                       const VoidDBusMethodCallback& callback) OVERRIDE;
  virtual void GetProperties(const dbus::ObjectPath& ipconfig_path,
                             const DictionaryValueCallback& callback) OVERRIDE;
  virtual base::DictionaryValue* CallGetPropertiesAndBlock(
      const dbus::ObjectPath& ipconfig_path) OVERRIDE;
  virtual void SetProperty(const dbus::ObjectPath& ipconfig_path,
                           const std::string& name,
                           const base::Value& value,
                           const VoidDBusMethodCallback& callback) OVERRIDE;
  virtual void ClearProperty(const dbus::ObjectPath& ipconfig_path,
                             const std::string& name,
                             const VoidDBusMethodCallback& callback) OVERRIDE;
  virtual void Remove(const dbus::ObjectPath& ipconfig_path,
                      const VoidDBusMethodCallback& callback) OVERRIDE;
  virtual bool CallRemoveAndBlock(
      const dbus::ObjectPath& ipconfig_path) OVERRIDE;

 private:
  typedef std::map<std::string, ShillClientHelper*> HelperMap;

  // Returns the corresponding ShillClientHelper for the profile.
  ShillClientHelper* GetHelper(const dbus::ObjectPath& ipconfig_path) {
    HelperMap::iterator it = helpers_.find(ipconfig_path.value());
    if (it != helpers_.end())
      return it->second;

    // There is no helper for the profile, create it.
    dbus::ObjectProxy* object_proxy =
        bus_->GetObjectProxy(flimflam::kFlimflamServiceName, ipconfig_path);
    ShillClientHelper* helper = new ShillClientHelper(bus_, object_proxy);
    helper->MonitorPropertyChanged(flimflam::kFlimflamIPConfigInterface);
    helpers_.insert(HelperMap::value_type(ipconfig_path.value(), helper));
    return helper;
  }

  dbus::Bus* bus_;
  HelperMap helpers_;
  STLValueDeleter<HelperMap> helpers_deleter_;

  DISALLOW_COPY_AND_ASSIGN(ShillIPConfigClientImpl);
};

ShillIPConfigClientImpl::ShillIPConfigClientImpl(dbus::Bus* bus)
    : bus_(bus),
      helpers_deleter_(&helpers_) {
}

void ShillIPConfigClientImpl::SetPropertyChangedHandler(
    const dbus::ObjectPath& ipconfig_path,
    const PropertyChangedHandler& handler) {
  GetHelper(ipconfig_path)->SetPropertyChangedHandler(handler);
}

void ShillIPConfigClientImpl::ResetPropertyChangedHandler(
    const dbus::ObjectPath& ipconfig_path) {
  GetHelper(ipconfig_path)->ResetPropertyChangedHandler();
}

void ShillIPConfigClientImpl::GetProperties(
    const dbus::ObjectPath& ipconfig_path,
    const DictionaryValueCallback& callback) {
  dbus::MethodCall method_call(flimflam::kFlimflamIPConfigInterface,
                               flimflam::kGetPropertiesFunction);
  GetHelper(ipconfig_path)->CallDictionaryValueMethod(&method_call, callback);
}

base::DictionaryValue* ShillIPConfigClientImpl::CallGetPropertiesAndBlock(
    const dbus::ObjectPath& ipconfig_path) {
  dbus::MethodCall method_call(flimflam::kFlimflamIPConfigInterface,
                               flimflam::kGetPropertiesFunction);
  return GetHelper(ipconfig_path)->CallDictionaryValueMethodAndBlock(
      &method_call);
}

void ShillIPConfigClientImpl::Refresh(
    const dbus::ObjectPath& ipconfig_path,
    const VoidDBusMethodCallback& callback) {
  dbus::MethodCall method_call(flimflam::kFlimflamIPConfigInterface,
                               shill::kRefreshFunction);
  GetHelper(ipconfig_path)->CallVoidMethod(&method_call, callback);
}

void ShillIPConfigClientImpl::SetProperty(
    const dbus::ObjectPath& ipconfig_path,
    const std::string& name,
    const base::Value& value,
    const VoidDBusMethodCallback& callback) {
  dbus::MethodCall method_call(flimflam::kFlimflamIPConfigInterface,
                               flimflam::kSetPropertyFunction);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(name);
  // IPConfig supports writing basic type and string array properties.
  switch (value.GetType()) {
    case base::Value::TYPE_LIST: {
      const base::ListValue* list_value = NULL;
      value.GetAsList(&list_value);
      dbus::MessageWriter variant_writer(NULL);
      writer.OpenVariant("as", &variant_writer);
      dbus::MessageWriter array_writer(NULL);
      variant_writer.OpenArray("s", &array_writer);
      for (base::ListValue::const_iterator it = list_value->begin();
           it != list_value->end();
           ++it) {
        DLOG_IF(ERROR, (*it)->GetType() != base::Value::TYPE_STRING)
            << "Unexpected type " << (*it)->GetType();
        std::string str;
        (*it)->GetAsString(&str);
        array_writer.AppendString(str);
      }
      variant_writer.CloseContainer(&array_writer);
      writer.CloseContainer(&variant_writer);
    }
    case base::Value::TYPE_BOOLEAN:
    case base::Value::TYPE_INTEGER:
    case base::Value::TYPE_DOUBLE:
    case base::Value::TYPE_STRING:
      dbus::AppendBasicTypeValueDataAsVariant(&writer, value);
      break;
    default:
      DLOG(ERROR) << "Unexpected type " << value.GetType();
  }
  GetHelper(ipconfig_path)->CallVoidMethod(&method_call, callback);
}

void ShillIPConfigClientImpl::ClearProperty(
    const dbus::ObjectPath& ipconfig_path,
    const std::string& name,
    const VoidDBusMethodCallback& callback) {
  dbus::MethodCall method_call(flimflam::kFlimflamIPConfigInterface,
                               flimflam::kClearPropertyFunction);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(name);
  GetHelper(ipconfig_path)->CallVoidMethod(&method_call, callback);
}

void ShillIPConfigClientImpl::Remove(
    const dbus::ObjectPath& ipconfig_path,
    const VoidDBusMethodCallback& callback) {
  dbus::MethodCall method_call(flimflam::kFlimflamIPConfigInterface,
                               flimflam::kRemoveConfigFunction);
  GetHelper(ipconfig_path)->CallVoidMethod(&method_call, callback);
}

bool ShillIPConfigClientImpl::CallRemoveAndBlock(
    const dbus::ObjectPath& ipconfig_path) {
  dbus::MethodCall method_call(flimflam::kFlimflamIPConfigInterface,
                               flimflam::kRemoveConfigFunction);
  return GetHelper(ipconfig_path)->CallVoidMethodAndBlock(&method_call);
}

// A stub implementation of ShillIPConfigClient.
class ShillIPConfigClientStubImpl : public ShillIPConfigClient {
 public:
  ShillIPConfigClientStubImpl() : weak_ptr_factory_(this) {}

  virtual ~ShillIPConfigClientStubImpl() {}

  ///////////////////////////////////////////////
  // ShillIPConfigClient overrides:
  virtual void SetPropertyChangedHandler(
      const dbus::ObjectPath& ipconfig_path,
      const PropertyChangedHandler& handler) OVERRIDE {}

  virtual void ResetPropertyChangedHandler(
      const dbus::ObjectPath& ipconfig_path) OVERRIDE {}

  virtual void Refresh(const dbus::ObjectPath& ipconfig_path,
                       const VoidDBusMethodCallback& callback) OVERRIDE {}

  virtual void GetProperties(const dbus::ObjectPath& ipconfig_path,
                             const DictionaryValueCallback& callback) OVERRIDE {
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&ShillIPConfigClientStubImpl::PassProperties,
                              weak_ptr_factory_.GetWeakPtr(),
                              callback));
  }

  virtual base::DictionaryValue* CallGetPropertiesAndBlock(
      const dbus::ObjectPath& ipconfig_path) OVERRIDE {
    return new base::DictionaryValue;
  }

  virtual void SetProperty(const dbus::ObjectPath& ipconfig_path,
                           const std::string& name,
                           const base::Value& value,
                           const VoidDBusMethodCallback& callback) OVERRIDE {
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS));
  }

  virtual void ClearProperty(const dbus::ObjectPath& ipconfig_path,
                             const std::string& name,
                             const VoidDBusMethodCallback& callback) OVERRIDE {
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS));
  }

  virtual void Remove(const dbus::ObjectPath& ipconfig_path,
                      const VoidDBusMethodCallback& callback) OVERRIDE {
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS));
  }

  virtual bool CallRemoveAndBlock(
      const dbus::ObjectPath& ipconfig_path) OVERRIDE {
    return true;
  }

 private:
  // Runs callback with |properties_|.
  void PassProperties(const DictionaryValueCallback& callback) const {
    callback.Run(DBUS_METHOD_CALL_SUCCESS, properties_);
  }

  base::DictionaryValue properties_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ShillIPConfigClientStubImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ShillIPConfigClientStubImpl);
};

}  // namespace

ShillIPConfigClient::ShillIPConfigClient() {}

ShillIPConfigClient::~ShillIPConfigClient() {}

// static
ShillIPConfigClient* ShillIPConfigClient::Create(
    DBusClientImplementationType type,
    dbus::Bus* bus) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION)
    return new ShillIPConfigClientImpl(bus);
  DCHECK_EQ(STUB_DBUS_CLIENT_IMPLEMENTATION, type);
  return new ShillIPConfigClientStubImpl();
}

}  // namespace chromeos
