// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_DBUS_CLIENT_IMPLEMENTATION_TYPE_H_
#define CHROMEOS_DBUS_DBUS_CLIENT_IMPLEMENTATION_TYPE_H_

namespace chromeos {

// An enum to describe the desired type of D-Bus client implemenation.
enum DBusClientImplementationType {
  REAL_DBUS_CLIENT_IMPLEMENTATION,  // The real implementation.
  STUB_DBUS_CLIENT_IMPLEMENTATION,  // A stub implementation.
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_DBUS_CLIENT_IMPLEMENTATION_TYPE_H_
