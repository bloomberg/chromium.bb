// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DBUS_FLIMFLAM_CLIENT_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_DBUS_FLIMFLAM_CLIENT_HELPER_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/dbus/dbus_method_call_status.h"

namespace base {

class Value;
class DictionaryValue;

}  // namespace base

namespace dbus {

class MethodCall;
class ObjectProxy;
class Response;
class Signal;

}  // namespace dbus

namespace chromeos {

// A class to help implement Flimflam clients.
class FlimflamClientHelper {
 public:
  explicit FlimflamClientHelper(dbus::ObjectProxy* proxy);

  virtual ~FlimflamClientHelper();

  // A callback to handle PropertyChanged signals.
  typedef base::Callback<void(const std::string& name,
                              const base::Value& value)> PropertyChangedHandler;

  // A callback to handle responses for methods without results.
  typedef base::Callback<void(DBusMethodCallStatus call_status)> VoidCallback;

  // A callback to handle responses for methods with DictionaryValue results.
  typedef base::Callback<void(
      DBusMethodCallStatus call_status,
      const base::DictionaryValue& result)> DictionaryValueCallback;

  // Sets PropertyChanged signal handler.
  void SetPropertyChangedHandler(const PropertyChangedHandler& handler);

  // Resets PropertyChanged signal handler.
  void ResetPropertyChangedHandler();

  // Starts monitoring PropertyChanged signal.
  void MonitorPropertyChanged(const std::string& interface_name);

  // Calls a method without results.
  void CallVoidMethod(dbus::MethodCall* method_call,
                      const VoidCallback& callback);

  // Calls a method with a dictionary value result.
  void CallDictionaryValueMethod(dbus::MethodCall* method_call,
                                 const DictionaryValueCallback& callback);

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

  base::WeakPtrFactory<FlimflamClientHelper> weak_ptr_factory_;
  dbus::ObjectProxy* proxy_;
  PropertyChangedHandler property_changed_handler_;

  DISALLOW_COPY_AND_ASSIGN(FlimflamClientHelper);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DBUS_FLIMFLAM_CLIENT_HELPER_H_
