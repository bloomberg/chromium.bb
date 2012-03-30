// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/flimflam_ipconfig_client.h"

#include "base/bind.h"
#include "base/chromeos/chromeos_version.h"
#include "base/message_loop.h"
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
  // Handles the result of signal connection setup.
  void OnSignalConnected(const std::string& interface,
                         const std::string& signal,
                         bool success);
  // Handles PropertyChanged signal.
  void OnPropertyChanged(dbus::Signal* signal);
  // Handles responses for methods without results.
  void OnVoidMethod(const VoidCallback& callback, dbus::Response* response);
  // Handles responses for methods with DictionaryValue results.
  void OnDictionaryValueMethod(const DictionaryValueCallback& callback,
                               dbus::Response* response);
  dbus::ObjectProxy* proxy_;
  base::WeakPtrFactory<FlimflamIPConfigClientImpl> weak_ptr_factory_;
  PropertyChangedHandler property_changed_handler_;

  DISALLOW_COPY_AND_ASSIGN(FlimflamIPConfigClientImpl);
};

FlimflamIPConfigClientImpl::FlimflamIPConfigClientImpl(dbus::Bus* bus)
    : proxy_(bus->GetObjectProxy(
        flimflam::kFlimflamServiceName,
        dbus::ObjectPath(flimflam::kFlimflamServicePath))),
      weak_ptr_factory_(this) {
  proxy_->ConnectToSignal(
      flimflam::kFlimflamIPConfigInterface,
      flimflam::kMonitorPropertyChanged,
      base::Bind(&FlimflamIPConfigClientImpl::OnPropertyChanged,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&FlimflamIPConfigClientImpl::OnSignalConnected,
                 weak_ptr_factory_.GetWeakPtr()));
}

void FlimflamIPConfigClientImpl::SetPropertyChangedHandler(
    const PropertyChangedHandler& handler) {
  property_changed_handler_ = handler;
}

void FlimflamIPConfigClientImpl::ResetPropertyChangedHandler() {
  property_changed_handler_.Reset();
}

// FlimflamIPConfigClient override.
void FlimflamIPConfigClientImpl::GetProperties(
    const DictionaryValueCallback& callback) {
  dbus::MethodCall method_call(flimflam::kFlimflamIPConfigInterface,
                               flimflam::kGetPropertiesFunction);
  proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::Bind(&FlimflamIPConfigClientImpl::OnDictionaryValueMethod,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
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
  proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                     base::Bind(&FlimflamIPConfigClientImpl::OnVoidMethod,
                                weak_ptr_factory_.GetWeakPtr(),
                                callback));
}

// FlimflamIPConfigClient override.
void FlimflamIPConfigClientImpl::ClearProperty(const std::string& name,
                                               const VoidCallback& callback) {
  dbus::MethodCall method_call(flimflam::kFlimflamIPConfigInterface,
                               flimflam::kClearPropertyFunction);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(name);
  proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                     base::Bind(&FlimflamIPConfigClientImpl::OnVoidMethod,
                                weak_ptr_factory_.GetWeakPtr(),
                                callback));
}

// FlimflamIPConfigClient override.
void FlimflamIPConfigClientImpl::Remove(const VoidCallback& callback) {
  dbus::MethodCall method_call(flimflam::kFlimflamIPConfigInterface,
                               flimflam::kRemoveConfigFunction);
  proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                     base::Bind(&FlimflamIPConfigClientImpl::OnVoidMethod,
                                weak_ptr_factory_.GetWeakPtr(),
                                callback));
}

void FlimflamIPConfigClientImpl::OnSignalConnected(const std::string& interface,
                                                   const std::string& signal,
                                                   bool success) {
  LOG_IF(ERROR, !success) << "Connect to " << interface << " "
                          << signal << " failed.";
}

void FlimflamIPConfigClientImpl::OnPropertyChanged(dbus::Signal* signal) {
  dbus::MessageReader reader(signal);
  std::string name;
  if (!reader.PopString(&name))
    return;
  scoped_ptr<base::Value> value(dbus::PopDataAsValue(&reader));
  if (!value.get())
    return;

  if (!property_changed_handler_.is_null())
    property_changed_handler_.Run(name, *value);
}

void FlimflamIPConfigClientImpl::OnVoidMethod(const VoidCallback& callback,
                                              dbus::Response* response) {
  if (!response) {
    callback.Run(FAILURE);
    return;
  }
  callback.Run(SUCCESS);
}

void FlimflamIPConfigClientImpl::OnDictionaryValueMethod(
    const DictionaryValueCallback& callback,
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
    MessageLoop::current()->PostTask(FROM_HERE, base::Bind(callback, SUCCESS));
  }

  // FlimflamIPConfigClient override.
  virtual void ClearProperty(const std::string& name,
                             const VoidCallback& callback) OVERRIDE {
    MessageLoop::current()->PostTask(FROM_HERE, base::Bind(callback, SUCCESS));
  }

  // FlimflamIPConfigClient override.
  virtual void Remove(const VoidCallback& callback) OVERRIDE {
    MessageLoop::current()->PostTask(FROM_HERE, base::Bind(callback, SUCCESS));
  }

 private:
  // Runs callback with |properties_|.
  void PassProperties(const DictionaryValueCallback& callback) const {
    callback.Run(SUCCESS, properties_);
  }

  base::WeakPtrFactory<FlimflamIPConfigClientStubImpl> weak_ptr_factory_;
  base::DictionaryValue properties_;

  DISALLOW_COPY_AND_ASSIGN(FlimflamIPConfigClientStubImpl);
};

}  // namespace

FlimflamIPConfigClient::FlimflamIPConfigClient() {}

FlimflamIPConfigClient::~FlimflamIPConfigClient() {}

// static
FlimflamIPConfigClient* FlimflamIPConfigClient::Create(dbus::Bus* bus) {
  if (base::chromeos::IsRunningOnChromeOS())
    return new FlimflamIPConfigClientImpl(bus);
  else
    return new FlimflamIPConfigClientStubImpl();
}

}  // namespace chromeos
