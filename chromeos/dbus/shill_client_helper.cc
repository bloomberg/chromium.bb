// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/shill_client_helper.h"

#include "base/bind.h"
#include "base/values.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "dbus/values_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

const char kInvalidResponseErrorName[] = "";  // No error name.
const char kInvalidResponseErrorMessage[] = "Invalid response.";

void OnBooleanMethodWithErrorCallback(
    const ShillClientHelper::BooleanCallback& callback,
    const ShillClientHelper::ErrorCallback& error_callback,
    dbus::Response* response) {
  if (!response) {
    error_callback.Run(kInvalidResponseErrorName, kInvalidResponseErrorMessage);
    return;
  }
  dbus::MessageReader reader(response);
  bool result;
  if (!reader.PopBool(&result)) {
    error_callback.Run(kInvalidResponseErrorName, kInvalidResponseErrorMessage);
    return;
  }
  callback.Run(result);
}

void OnStringMethodWithErrorCallback(
    const ShillClientHelper::StringCallback& callback,
    const ShillClientHelper::ErrorCallback& error_callback,
    dbus::Response* response) {
  if (!response) {
    error_callback.Run(kInvalidResponseErrorName, kInvalidResponseErrorMessage);
    return;
  }
  dbus::MessageReader reader(response);
  std::string result;
  if (!reader.PopString(&result)) {
    error_callback.Run(kInvalidResponseErrorName, kInvalidResponseErrorMessage);
    return;
  }
  callback.Run(result);
}

// Handles responses for methods without results.
void OnVoidMethod(const VoidDBusMethodCallback& callback,
                  dbus::Response* response) {
  if (!response) {
    callback.Run(DBUS_METHOD_CALL_FAILURE);
    return;
  }
  callback.Run(DBUS_METHOD_CALL_SUCCESS);
}

// Handles responses for methods with ObjectPath results.
void OnObjectPathMethod(
    const ObjectPathDBusMethodCallback& callback,
    dbus::Response* response) {
  if (!response) {
    callback.Run(DBUS_METHOD_CALL_FAILURE, dbus::ObjectPath());
    return;
  }
  dbus::MessageReader reader(response);
  dbus::ObjectPath result;
  if (!reader.PopObjectPath(&result)) {
    callback.Run(DBUS_METHOD_CALL_FAILURE, dbus::ObjectPath());
    return;
  }
  callback.Run(DBUS_METHOD_CALL_SUCCESS, result);
}

// Handles responses for methods with ObjectPath results and no status.
void OnObjectPathMethodWithoutStatus(
    const ObjectPathCallback& callback,
    const ShillClientHelper::ErrorCallback& error_callback,
    dbus::Response* response) {
  if (!response) {
    error_callback.Run(kInvalidResponseErrorName, kInvalidResponseErrorMessage);
    return;
  }
  dbus::MessageReader reader(response);
  dbus::ObjectPath result;
  if (!reader.PopObjectPath(&result)) {
    error_callback.Run(kInvalidResponseErrorName, kInvalidResponseErrorMessage);
    return;
  }
  callback.Run(result);
}

// Handles responses for methods with DictionaryValue results.
void OnDictionaryValueMethod(
    const ShillClientHelper::DictionaryValueCallback& callback,
    dbus::Response* response) {
  if (!response) {
    base::DictionaryValue result;
    callback.Run(DBUS_METHOD_CALL_FAILURE, result);
    return;
  }
  dbus::MessageReader reader(response);
  scoped_ptr<base::Value> value(dbus::PopDataAsValue(&reader));
  base::DictionaryValue* result = NULL;
  if (!value.get() || !value->GetAsDictionary(&result)) {
    base::DictionaryValue result;
    callback.Run(DBUS_METHOD_CALL_FAILURE, result);
    return;
  }
  callback.Run(DBUS_METHOD_CALL_SUCCESS, *result);
}

// Handles responses for methods without results.
void OnVoidMethodWithErrorCallback(
    const base::Closure& callback,
    dbus::Response* response) {
  callback.Run();
}

// Handles responses for methods with DictionaryValue results.
// Used by CallDictionaryValueMethodWithErrorCallback().
void OnDictionaryValueMethodWithErrorCallback(
    const ShillClientHelper::DictionaryValueCallbackWithoutStatus& callback,
    const ShillClientHelper::ErrorCallback& error_callback,
    dbus::Response* response) {
  dbus::MessageReader reader(response);
  scoped_ptr<base::Value> value(dbus::PopDataAsValue(&reader));
  base::DictionaryValue* result = NULL;
  if (!value.get() || !value->GetAsDictionary(&result)) {
    error_callback.Run(kInvalidResponseErrorName, kInvalidResponseErrorMessage);
    return;
  }
  callback.Run(*result);
}

// Handles responses for methods with ListValue results.
void OnListValueMethodWithErrorCallback(
    const ShillClientHelper::ListValueCallback& callback,
    const ShillClientHelper::ErrorCallback& error_callback,
    dbus::Response* response) {
  dbus::MessageReader reader(response);
  scoped_ptr<base::Value> value(dbus::PopDataAsValue(&reader));
  base::ListValue* result = NULL;
  if (!value.get() || !value->GetAsList(&result)) {
    error_callback.Run(kInvalidResponseErrorName, kInvalidResponseErrorMessage);
    return;
  }
  callback.Run(*result);
}

// Handles running appropriate error callbacks.
void OnError(const ShillClientHelper::ErrorCallback& error_callback,
             dbus::ErrorResponse* response) {
  std::string error_name;
  std::string error_message;
  if (response) {
    // Error message may contain the error message as string.
    dbus::MessageReader reader(response);
    error_name = response->GetErrorName();
    reader.PopString(&error_message);
  }
  error_callback.Run(error_name, error_message);
}

}  // namespace

ShillClientHelper::ShillClientHelper(dbus::Bus* bus,
                                     dbus::ObjectProxy* proxy)
    : blocking_method_caller_(bus, proxy),
      proxy_(proxy),
      weak_ptr_factory_(this) {
}

ShillClientHelper::~ShillClientHelper() {
  LOG_IF(ERROR, observer_list_.size() != 0u)
      << "ShillClientHelper destroyed with active observers: "
      << observer_list_.size();
}

void ShillClientHelper::AddPropertyChangedObserver(
    ShillPropertyChangedObserver* observer) {
  // Excecute all the pending MonitorPropertyChanged calls.
  for (size_t i = 0; i < interfaces_to_be_monitored_.size(); ++i) {
    MonitorPropertyChangedInternal(interfaces_to_be_monitored_[i]);
  }
  interfaces_to_be_monitored_.clear();

  observer_list_.AddObserver(observer);
}

void ShillClientHelper::RemovePropertyChangedObserver(
    ShillPropertyChangedObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void ShillClientHelper::MonitorPropertyChanged(
    const std::string& interface_name) {
  if (observer_list_.size() > 0) {
    // Effectively monitor the PropertyChanged now.
    MonitorPropertyChangedInternal(interface_name);
  } else {
    // Delay the ConnectToSignal until an observer is added.
    interfaces_to_be_monitored_.push_back(interface_name);
  }
}

void ShillClientHelper::MonitorPropertyChangedInternal(
    const std::string& interface_name) {
  // We are not using dbus::PropertySet to monitor PropertyChanged signal
  // because the interface is not "org.freedesktop.DBus.Properties".
  proxy_->ConnectToSignal(interface_name,
                          flimflam::kMonitorPropertyChanged,
                          base::Bind(&ShillClientHelper::OnPropertyChanged,
                                     weak_ptr_factory_.GetWeakPtr()),
                          base::Bind(&ShillClientHelper::OnSignalConnected,
                                     weak_ptr_factory_.GetWeakPtr()));
}

void ShillClientHelper::CallVoidMethod(
    dbus::MethodCall* method_call,
    const VoidDBusMethodCallback& callback) {
  proxy_->CallMethod(method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                     base::Bind(&OnVoidMethod,
                                callback));
}

void ShillClientHelper::CallObjectPathMethod(
    dbus::MethodCall* method_call,
    const ObjectPathDBusMethodCallback& callback) {
  proxy_->CallMethod(method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                     base::Bind(&OnObjectPathMethod,
                                callback));
}

void ShillClientHelper::CallObjectPathMethodWithErrorCallback(
    dbus::MethodCall* method_call,
    const ObjectPathCallback& callback,
    const ErrorCallback& error_callback) {
  proxy_->CallMethodWithErrorCallback(
      method_call,
      dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::Bind(&OnObjectPathMethodWithoutStatus,
                 callback,
                 error_callback),
      base::Bind(&OnError,
                 error_callback));
}

void ShillClientHelper::CallDictionaryValueMethod(
    dbus::MethodCall* method_call,
    const DictionaryValueCallback& callback) {
  proxy_->CallMethod(method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                     base::Bind(&OnDictionaryValueMethod,
                                callback));
}

void ShillClientHelper::CallVoidMethodWithErrorCallback(
    dbus::MethodCall* method_call,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  proxy_->CallMethodWithErrorCallback(
      method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::Bind(&OnVoidMethodWithErrorCallback,
                 callback),
      base::Bind(&OnError,
                 error_callback));
}

void ShillClientHelper::CallBooleanMethodWithErrorCallback(
    dbus::MethodCall* method_call,
    const BooleanCallback& callback,
    const ErrorCallback& error_callback) {
  proxy_->CallMethodWithErrorCallback(
      method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::Bind(&OnBooleanMethodWithErrorCallback,
                 callback,
                 error_callback),
      base::Bind(&OnError,
                 error_callback));
}

void ShillClientHelper::CallStringMethodWithErrorCallback(
    dbus::MethodCall* method_call,
    const StringCallback& callback,
    const ErrorCallback& error_callback) {
  proxy_->CallMethodWithErrorCallback(
      method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::Bind(&OnStringMethodWithErrorCallback,
                 callback,
                 error_callback),
      base::Bind(&OnError,
                 error_callback));
}

void ShillClientHelper::CallDictionaryValueMethodWithErrorCallback(
    dbus::MethodCall* method_call,
    const DictionaryValueCallbackWithoutStatus& callback,
    const ErrorCallback& error_callback) {
  proxy_->CallMethodWithErrorCallback(
      method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::Bind(
          &OnDictionaryValueMethodWithErrorCallback,
          callback,
          error_callback),
      base::Bind(&OnError,
                 error_callback));
}

void ShillClientHelper::CallListValueMethodWithErrorCallback(
    dbus::MethodCall* method_call,
    const ListValueCallback& callback,
    const ErrorCallback& error_callback) {
  proxy_->CallMethodWithErrorCallback(
      method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::Bind(
          &OnListValueMethodWithErrorCallback,
          callback,
          error_callback),
      base::Bind(&OnError,
                 error_callback));
}

bool ShillClientHelper::CallVoidMethodAndBlock(
    dbus::MethodCall* method_call) {
  scoped_ptr<dbus::Response> response(
      blocking_method_caller_.CallMethodAndBlock(method_call));
  if (!response.get())
    return false;
  return true;
}

base::DictionaryValue* ShillClientHelper::CallDictionaryValueMethodAndBlock(
    dbus::MethodCall* method_call) {
  scoped_ptr<dbus::Response> response(
      blocking_method_caller_.CallMethodAndBlock(method_call));
  if (!response.get())
    return NULL;

  dbus::MessageReader reader(response.get());
  base::Value* value = dbus::PopDataAsValue(&reader);
  base::DictionaryValue* result = NULL;
  if (!value || !value->GetAsDictionary(&result)) {
    delete value;
    return NULL;
  }
  return result;
}

// static
void ShillClientHelper::AppendValueDataAsVariant(dbus::MessageWriter* writer,
                                                 const base::Value& value) {
  // Support basic types and string-to-string dictionary.
  switch (value.GetType()) {
    case base::Value::TYPE_DICTIONARY: {
      const base::DictionaryValue* dictionary = NULL;
      value.GetAsDictionary(&dictionary);
      dbus::MessageWriter variant_writer(NULL);
      writer->OpenVariant("a{ss}", &variant_writer);
      dbus::MessageWriter array_writer(NULL);
      variant_writer.OpenArray("{ss}", &array_writer);
      for (base::DictionaryValue::Iterator it(*dictionary);
           it.HasNext();
           it.Advance()) {
        dbus::MessageWriter entry_writer(NULL);
        array_writer.OpenDictEntry(&entry_writer);
        entry_writer.AppendString(it.key());
        const base::Value& value = it.value();
        std::string value_string;
        DLOG_IF(ERROR, value.GetType() != base::Value::TYPE_STRING)
            << "Unexpected type " << value.GetType();
        value.GetAsString(&value_string);
        entry_writer.AppendString(value_string);
        array_writer.CloseContainer(&entry_writer);
      }
      variant_writer.CloseContainer(&array_writer);
      writer->CloseContainer(&variant_writer);
      break;
    }
    case base::Value::TYPE_BOOLEAN:
    case base::Value::TYPE_INTEGER:
    case base::Value::TYPE_DOUBLE:
    case base::Value::TYPE_STRING:
      dbus::AppendBasicTypeValueDataAsVariant(writer, value);
      break;
    default:
      DLOG(ERROR) << "Unexpected type " << value.GetType();
  }

}

void ShillClientHelper::OnSignalConnected(const std::string& interface,
                                          const std::string& signal,
                                          bool success) {
  LOG_IF(ERROR, !success) << "Connect to " << interface << " " << signal
                          << " failed.";
}

void ShillClientHelper::OnPropertyChanged(dbus::Signal* signal) {
  if (!observer_list_.might_have_observers())
    return;

  dbus::MessageReader reader(signal);
  std::string name;
  if (!reader.PopString(&name))
    return;
  scoped_ptr<base::Value> value(dbus::PopDataAsValue(&reader));
  if (!value.get())
    return;

  FOR_EACH_OBSERVER(ShillPropertyChangedObserver, observer_list_,
                    OnPropertyChanged(name, *value));
}


}  // namespace chromeos
