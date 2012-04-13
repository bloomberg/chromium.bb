// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/flimflam_ipconfig_client.h"

#include "base/bind.h"
#include "base/message_loop.h"
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

  // FlimflamIPConfigClient override.
  virtual void SetPropertyChangedHandler(
      const PropertyChangedHandler& handler) OVERRIDE;

  // FlimflamIPConfigClient override.
  virtual void ResetPropertyChangedHandler() OVERRIDE;
  // FlimflamIPConfigClient override.
  virtual void GetProperties(const DictionaryValueCallback& callback) OVERRIDE;
  // FlimflamIPConfigClient override.
  virtual void SetProperty(const std::string& name,
                           const base::Value& value,
                           const VoidCallback& callback) OVERRIDE;
  // FlimflamIPConfigClient override.
  virtual void ClearProperty(const std::string& name,
                             const VoidCallback& callback) OVERRIDE;
  // FlimflamIPConfigClient override.
  virtual void Remove(const VoidCallback& callback) OVERRIDE;

 private:
  dbus::ObjectProxy* proxy_;
  FlimflamClientHelper helper_;

  DISALLOW_COPY_AND_ASSIGN(FlimflamIPConfigClientImpl);
};

FlimflamIPConfigClientImpl::FlimflamIPConfigClientImpl(dbus::Bus* bus)
    : proxy_(bus->GetObjectProxy(
        flimflam::kFlimflamServiceName,
        dbus::ObjectPath(flimflam::kFlimflamServicePath))),
      helper_(proxy_) {
  helper_.MonitorPropertyChanged(flimflam::kFlimflamIPConfigInterface);
}

void FlimflamIPConfigClientImpl::SetPropertyChangedHandler(
    const PropertyChangedHandler& handler) {
  helper_.SetPropertyChangedHandler(handler);
}

void FlimflamIPConfigClientImpl::ResetPropertyChangedHandler() {
  helper_.ResetPropertyChangedHandler();
}

// FlimflamIPConfigClient override.
void FlimflamIPConfigClientImpl::GetProperties(
    const DictionaryValueCallback& callback) {
  dbus::MethodCall method_call(flimflam::kFlimflamIPConfigInterface,
                               flimflam::kGetPropertiesFunction);
  helper_.CallDictionaryValueMethod(&method_call, callback);
}

// FlimflamIPConfigClient override.
void FlimflamIPConfigClientImpl::SetProperty(const std::string& name,
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
  helper_.CallVoidMethod(&method_call, callback);
}

// FlimflamIPConfigClient override.
void FlimflamIPConfigClientImpl::ClearProperty(const std::string& name,
                                               const VoidCallback& callback) {
  dbus::MethodCall method_call(flimflam::kFlimflamIPConfigInterface,
                               flimflam::kClearPropertyFunction);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(name);
  helper_.CallVoidMethod(&method_call, callback);
}

// FlimflamIPConfigClient override.
void FlimflamIPConfigClientImpl::Remove(const VoidCallback& callback) {
  dbus::MethodCall method_call(flimflam::kFlimflamIPConfigInterface,
                               flimflam::kRemoveConfigFunction);
  helper_.CallVoidMethod(&method_call, callback);
}

// A stub implementation of FlimflamIPConfigClient.
class FlimflamIPConfigClientStubImpl : public FlimflamIPConfigClient {
 public:
  FlimflamIPConfigClientStubImpl() : weak_ptr_factory_(this) {}

  virtual ~FlimflamIPConfigClientStubImpl() {}

  // FlimflamIPConfigClient override.
  virtual void SetPropertyChangedHandler(
      const PropertyChangedHandler& handler) OVERRIDE {}

  // FlimflamIPConfigClient override.
  virtual void ResetPropertyChangedHandler() OVERRIDE {}

  // FlimflamIPConfigClient override.
  virtual void GetProperties(const DictionaryValueCallback& callback) OVERRIDE {
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&FlimflamIPConfigClientStubImpl::PassProperties,
                              weak_ptr_factory_.GetWeakPtr(),
                              callback));
  }

  // FlimflamIPConfigClient override.
  virtual void SetProperty(const std::string& name,
                           const base::Value& value,
                           const VoidCallback& callback) OVERRIDE {
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS));
  }

  // FlimflamIPConfigClient override.
  virtual void ClearProperty(const std::string& name,
                             const VoidCallback& callback) OVERRIDE {
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS));
  }

  // FlimflamIPConfigClient override.
  virtual void Remove(const VoidCallback& callback) OVERRIDE {
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS));
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
