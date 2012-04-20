// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/flimflam_ipconfig_client.h"

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

// The FlimflamIPConfigClient implementation.
class FlimflamIPConfigClientImpl : public FlimflamIPConfigClient {
 public:
  explicit FlimflamIPConfigClientImpl(dbus::Bus* bus);

  // FlimflamIPConfigClient overrides:
  virtual void SetPropertyChangedHandler(
      const dbus::ObjectPath& ipconfig_path,
      const PropertyChangedHandler& handler) OVERRIDE;
  virtual void ResetPropertyChangedHandler(
      const dbus::ObjectPath& ipconfig_path) OVERRIDE;
  virtual void GetProperties(const dbus::ObjectPath& ipconfig_path,
                             const DictionaryValueCallback& callback) OVERRIDE;
  virtual base::DictionaryValue* CallGetPropertiesAndBlock(
      const dbus::ObjectPath& ipconfig_path) OVERRIDE;
  virtual void SetProperty(const dbus::ObjectPath& ipconfig_path,
                           const std::string& name,
                           const base::Value& value,
                           const VoidCallback& callback) OVERRIDE;
  virtual void ClearProperty(const dbus::ObjectPath& ipconfig_path,
                             const std::string& name,
                             const VoidCallback& callback) OVERRIDE;
  virtual void Remove(const dbus::ObjectPath& ipconfig_path,
                      const VoidCallback& callback) OVERRIDE;
  virtual bool CallRemoveAndBlock(
      const dbus::ObjectPath& ipconfig_path) OVERRIDE;

 private:
  typedef std::map<std::string, FlimflamClientHelper*> HelperMap;

  // Returns the corresponding FlimflamClientHelper for the profile.
  FlimflamClientHelper* GetHelper(const dbus::ObjectPath& ipconfig_path) {
    HelperMap::iterator it = helpers_.find(ipconfig_path.value());
    if (it != helpers_.end())
      return it->second;

    // There is no helper for the profile, create it.
    dbus::ObjectProxy* object_proxy =
        bus_->GetObjectProxy(flimflam::kFlimflamServiceName, ipconfig_path);
    FlimflamClientHelper* helper = new FlimflamClientHelper(bus_, object_proxy);
    helper->MonitorPropertyChanged(flimflam::kFlimflamIPConfigInterface);
    helpers_.insert(HelperMap::value_type(ipconfig_path.value(), helper));
    return helper;
  }

  dbus::Bus* bus_;
  HelperMap helpers_;
  STLValueDeleter<HelperMap> helpers_deleter_;

  DISALLOW_COPY_AND_ASSIGN(FlimflamIPConfigClientImpl);
};

FlimflamIPConfigClientImpl::FlimflamIPConfigClientImpl(dbus::Bus* bus)
    : bus_(bus),
      helpers_deleter_(&helpers_) {
}

void FlimflamIPConfigClientImpl::SetPropertyChangedHandler(
    const dbus::ObjectPath& ipconfig_path,
    const PropertyChangedHandler& handler) {
  GetHelper(ipconfig_path)->SetPropertyChangedHandler(handler);
}

void FlimflamIPConfigClientImpl::ResetPropertyChangedHandler(
    const dbus::ObjectPath& ipconfig_path) {
  GetHelper(ipconfig_path)->ResetPropertyChangedHandler();
}

void FlimflamIPConfigClientImpl::GetProperties(
    const dbus::ObjectPath& ipconfig_path,
    const DictionaryValueCallback& callback) {
  dbus::MethodCall method_call(flimflam::kFlimflamIPConfigInterface,
                               flimflam::kGetPropertiesFunction);
  GetHelper(ipconfig_path)->CallDictionaryValueMethod(&method_call, callback);
}

base::DictionaryValue* FlimflamIPConfigClientImpl::CallGetPropertiesAndBlock(
    const dbus::ObjectPath& ipconfig_path) {
  dbus::MethodCall method_call(flimflam::kFlimflamIPConfigInterface,
                               flimflam::kGetPropertiesFunction);
  return GetHelper(ipconfig_path)->CallDictionaryValueMethodAndBlock(
      &method_call);
}

void FlimflamIPConfigClientImpl::SetProperty(
    const dbus::ObjectPath& ipconfig_path,
    const std::string& name,
    const base::Value& value,
    const VoidCallback& callback) {
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

void FlimflamIPConfigClientImpl::ClearProperty(
    const dbus::ObjectPath& ipconfig_path,
    const std::string& name,
    const VoidCallback& callback) {
  dbus::MethodCall method_call(flimflam::kFlimflamIPConfigInterface,
                               flimflam::kClearPropertyFunction);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(name);
  GetHelper(ipconfig_path)->CallVoidMethod(&method_call, callback);
}

void FlimflamIPConfigClientImpl::Remove(const dbus::ObjectPath& ipconfig_path,
                                        const VoidCallback& callback) {
  dbus::MethodCall method_call(flimflam::kFlimflamIPConfigInterface,
                               flimflam::kRemoveConfigFunction);
  GetHelper(ipconfig_path)->CallVoidMethod(&method_call, callback);
}

bool FlimflamIPConfigClientImpl::CallRemoveAndBlock(
    const dbus::ObjectPath& ipconfig_path) {
  dbus::MethodCall method_call(flimflam::kFlimflamIPConfigInterface,
                               flimflam::kRemoveConfigFunction);
  return GetHelper(ipconfig_path)->CallVoidMethodAndBlock(&method_call);
}

// A stub implementation of FlimflamIPConfigClient.
class FlimflamIPConfigClientStubImpl : public FlimflamIPConfigClient {
 public:
  FlimflamIPConfigClientStubImpl() : weak_ptr_factory_(this) {}

  virtual ~FlimflamIPConfigClientStubImpl() {}

  // FlimflamIPConfigClient override.
  virtual void SetPropertyChangedHandler(
      const dbus::ObjectPath& ipconfig_path,
      const PropertyChangedHandler& handler) OVERRIDE {}

  // FlimflamIPConfigClient override.
  virtual void ResetPropertyChangedHandler(
      const dbus::ObjectPath& ipconfig_path) OVERRIDE {}

  // FlimflamIPConfigClient override.
  virtual void GetProperties(const dbus::ObjectPath& ipconfig_path,
                             const DictionaryValueCallback& callback) OVERRIDE {
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&FlimflamIPConfigClientStubImpl::PassProperties,
                              weak_ptr_factory_.GetWeakPtr(),
                              callback));
  }

  // FlimflamIPConfigClient override.
  virtual base::DictionaryValue* CallGetPropertiesAndBlock(
      const dbus::ObjectPath& ipconfig_path) OVERRIDE {
    return new base::DictionaryValue;
  }

  // FlimflamIPConfigClient override.
  virtual void SetProperty(const dbus::ObjectPath& ipconfig_path,
                           const std::string& name,
                           const base::Value& value,
                           const VoidCallback& callback) OVERRIDE {
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS));
  }

  // FlimflamIPConfigClient override.
  virtual void ClearProperty(const dbus::ObjectPath& ipconfig_path,
                             const std::string& name,
                             const VoidCallback& callback) OVERRIDE {
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS));
  }

  // FlimflamIPConfigClient override.
  virtual void Remove(const dbus::ObjectPath& ipconfig_path,
                      const VoidCallback& callback) OVERRIDE {
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS));
  }

  // FlimflamIPConfigClient override.
  virtual bool CallRemoveAndBlock(
      const dbus::ObjectPath& ipconfig_path) OVERRIDE {
    return true;
  }

 private:
  // Runs callback with |properties_|.
  void PassProperties(const DictionaryValueCallback& callback) const {
    callback.Run(DBUS_METHOD_CALL_SUCCESS, properties_);
  }

  base::WeakPtrFactory<FlimflamIPConfigClientStubImpl> weak_ptr_factory_;
  base::DictionaryValue properties_;

  DISALLOW_COPY_AND_ASSIGN(FlimflamIPConfigClientStubImpl);
};

}  // namespace

FlimflamIPConfigClient::FlimflamIPConfigClient() {}

FlimflamIPConfigClient::~FlimflamIPConfigClient() {}

// static
FlimflamIPConfigClient* FlimflamIPConfigClient::Create(
    DBusClientImplementationType type,
    dbus::Bus* bus) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION)
    return new FlimflamIPConfigClientImpl(bus);
  DCHECK_EQ(STUB_DBUS_CLIENT_IMPLEMENTATION, type);
  return new FlimflamIPConfigClientStubImpl();
}

}  // namespace chromeos
