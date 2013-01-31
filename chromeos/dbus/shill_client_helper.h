// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SHILL_CLIENT_HELPER_H_
#define CHROMEOS_DBUS_SHILL_CLIENT_HELPER_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "chromeos/dbus/blocking_method_caller.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/shill_property_changed_observer.h"

namespace base {

class Value;
class DictionaryValue;

}  // namespace base

namespace dbus {

class Bus;
class ErrorResponse;
class MessageWriter;
class MethodCall;
class ObjectPath;
class ObjectProxy;
class Response;
class Signal;

}  // namespace dbus

namespace chromeos {

// A class to help implement Shill clients.
class ShillClientHelper {
 public:
  // A callback to handle PropertyChanged signals.
  typedef base::Callback<void(const std::string& name,
                              const base::Value& value)> PropertyChangedHandler;

  // A callback to handle responses for methods with DictionaryValue results.
  typedef base::Callback<void(
      DBusMethodCallStatus call_status,
      const base::DictionaryValue& result)> DictionaryValueCallback;

  // A callback to handle responses for methods with DictionaryValue results.
  // This is used by CallDictionaryValueMethodWithErrorCallback.
  typedef base::Callback<void(const base::DictionaryValue& result)>
      DictionaryValueCallbackWithoutStatus;

  // A callback to handle responses of methods returning a ListValue.
  typedef base::Callback<void(const base::ListValue& result)> ListValueCallback;

  // A callback to handle errors for method call.
  typedef base::Callback<void(const std::string& error_name,
                              const std::string& error_message)> ErrorCallback;

  ShillClientHelper(dbus::Bus* bus, dbus::ObjectProxy* proxy);

  virtual ~ShillClientHelper();

  // Adds an |observer| of the PropertyChanged signal.
  void AddPropertyChangedObserver(ShillPropertyChangedObserver* observer);

  // Removes an |observer| of the PropertyChanged signal.
  void RemovePropertyChangedObserver(ShillPropertyChangedObserver* observer);

  // Starts monitoring PropertyChanged signal.
  void MonitorPropertyChanged(const std::string& interface_name);

  // Calls a method without results.
  void CallVoidMethod(dbus::MethodCall* method_call,
                      const VoidDBusMethodCallback& callback);

  // Calls a method with an object path result.
  void CallObjectPathMethod(dbus::MethodCall* method_call,
                            const ObjectPathDBusMethodCallback& callback);

  // Calls a method with an object path result where there is an error callback.
  void CallObjectPathMethodWithErrorCallback(
      dbus::MethodCall* method_call,
      const ObjectPathCallback& callback,
      const ErrorCallback& error_callback);

  // Calls a method with a dictionary value result.
  void CallDictionaryValueMethod(dbus::MethodCall* method_call,
                                 const DictionaryValueCallback& callback);

  // Calls a method without results with error callback.
  void CallVoidMethodWithErrorCallback(dbus::MethodCall* method_call,
                                       const base::Closure& callback,
                                       const ErrorCallback& error_callback);

  // Calls a method with a dictionary value result with error callback.
  void CallDictionaryValueMethodWithErrorCallback(
      dbus::MethodCall* method_call,
      const DictionaryValueCallbackWithoutStatus& callback,
      const ErrorCallback& error_callback);

  // Calls a method with a boolean array result with error callback.
  void CallListValueMethodWithErrorCallback(
      dbus::MethodCall* method_call,
      const ListValueCallback& callback,
      const ErrorCallback& error_callback);

  // DEPRECATED DO NOT USE: Calls a method without results.
  bool CallVoidMethodAndBlock(dbus::MethodCall* method_call);

  // DEPRECATED DO NOT USE: Calls a method with a dictionary value result.
  // The caller is responsible to delete the result.
  // This method returns NULL when method call fails.
  base::DictionaryValue* CallDictionaryValueMethodAndBlock(
      dbus::MethodCall* method_call);

  // Appends the value (basic types and string-to-string dictionary) to the
  // writer as a variant.
  static void AppendValueDataAsVariant(dbus::MessageWriter* writer,
                                       const base::Value& value);

 private:
  // Handles the result of signal connection setup.
  void OnSignalConnected(const std::string& interface,
                         const std::string& signal,
                         bool success);

  // Handles PropertyChanged signal.
  void OnPropertyChanged(dbus::Signal* signal);

  // Handles responses for methods without results.
  void OnVoidMethod(const VoidDBusMethodCallback& callback,
                    dbus::Response* response);

  // Handles responses for methods with ObjectPath results.
  void OnObjectPathMethod(const ObjectPathDBusMethodCallback& callback,
                          dbus::Response* response);

  // Handles responses for methods with ObjectPath results.
  void OnObjectPathMethodWithoutStatus(
      const ObjectPathCallback& callback,
      const ErrorCallback& error_callback,
      dbus::Response* response);

  // Handles responses for methods with DictionaryValue results.
  void OnDictionaryValueMethod(const DictionaryValueCallback& callback,
                               dbus::Response* response);

  // Handles responses for methods without results.
  // Used by CallVoidMethodWithErrorCallback().
  void OnVoidMethodWithErrorCallback(const base::Closure& callback,
                                     dbus::Response* response);

  // Handles responses for methods with DictionaryValue results.
  // Used by CallDictionaryValueMethodWithErrorCallback().
  void OnDictionaryValueMethodWithErrorCallback(
      const DictionaryValueCallbackWithoutStatus& callback,
      const ErrorCallback& error_callback,
      dbus::Response* response);

  // Handles responses for methods with Boolean array results.
  // Used by CallBooleanArrayMethodWithErrorCallback().
  void OnListValueMethodWithErrorCallback(
      const ListValueCallback& callback,
      const ErrorCallback& error_callback,
      dbus::Response* response);

  // Handles errors for method calls.
  void OnError(const ErrorCallback& error_callback,
               dbus::ErrorResponse* response);

  // TODO(hashimoto): Remove this when we no longer need to make blocking calls.
  BlockingMethodCaller blocking_method_caller_;
  dbus::ObjectProxy* proxy_;
  PropertyChangedHandler property_changed_handler_;
  ObserverList<ShillPropertyChangedObserver, true /* check_empty */>
      observer_list_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ShillClientHelper> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ShillClientHelper);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SHILL_CLIENT_HELPER_H_
