// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_DBUS_METHOD_CALL_STATUS_H_
#define CHROMEOS_DBUS_DBUS_METHOD_CALL_STATUS_H_

#include "base/callback.h"

namespace dbus {

class ObjectPath;

}  // namespace dbus

namespace chromeos {

// An enum to describe whether or not a DBus method call succeeded.
enum DBusMethodCallStatus {
  DBUS_METHOD_CALL_FAILURE,
  DBUS_METHOD_CALL_SUCCESS,
};

// A callback to handle responses of methods without results.
typedef base::Callback<void(
    DBusMethodCallStatus call_status)> VoidDBusMethodCallback;

// A callback to handle responses of methods returning a bool value.
typedef base::Callback<void(DBusMethodCallStatus call_status,
                            bool result)> BoolDBusMethodCallback;

// A callback to handle responses of methods returning a string value.
typedef base::Callback<void(
    DBusMethodCallStatus call_status,
    const std::string& result)> StringDBusMethodCallback;

// A callback to handle responses of methods returning a boolean value.
typedef base::Callback<void(
    DBusMethodCallStatus call_status,
    bool result)> BooleanDBusMethodCallback;

// A callback to handle responses of methods returning a ObjectPath value.
typedef base::Callback<void(
    DBusMethodCallStatus call_status,
    const dbus::ObjectPath& result)> ObjectPathDBusMethodCallback;

// A callback to handle responses of methods returning a ObjectPath value that
// doesn't get call status.
typedef base::Callback<void(const dbus::ObjectPath& result)> ObjectPathCallback;

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_DBUS_METHOD_CALL_STATUS_H_
