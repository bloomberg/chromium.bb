// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_DBUS_CLIENT_TYPES_H_
#define CHROMEOS_DBUS_DBUS_CLIENT_TYPES_H_

#include <string>

#include "chromeos/chromeos_export.h"

namespace chromeos {

// NOTE: When adding a new client type also add a string name in the .cc file.
enum class DBusClientType : int {
  NONE = 0,
  BLUETOOTH = 1 << 0,
  CRAS = 1 << 1,
  CROS_DISKS = 1 << 2,
  CRYPTOHOME = 1 << 3,
  DEBUG_DAEMON = 1 << 4,
  EASY_UNLOCK = 1 << 5,
  LORGNETTE_MANAGER = 1 << 6,
  SHILL = 1 << 7,
  GSM_SMS = 1 << 8,
  IMAGE_BURNER = 1 << 9,
  MODEM_MESSAGING = 1 << 10,
  PERMISSION_BROKER = 1 << 11,
  POWER_MANAGER = 1 << 12,
  SESSION_MANAGER = 1 << 13,
  SMS = 1 << 14,
  SYSTEM_CLOCK = 1 << 15,
  UPDATE_ENGINE = 1 << 16,
  ARC_OBB_MOUNTER = 1 << 17,
  ALL = ~NONE,
};

using DBusClientTypeMask = int;

// An enum to describe the desired type of D-Bus client implemenation.
enum DBusClientImplementationType {
  REAL_DBUS_CLIENT_IMPLEMENTATION,
  FAKE_DBUS_CLIENT_IMPLEMENTATION,
};

// Parses command line param values for dbus subsystems that should be forced
// to use real implementations, comma-separated like "bluetooth,cras,shill".
// See the .cc file for the client names.
CHROMEOS_EXPORT DBusClientTypeMask
ParseDBusRealClientsList(const std::string& clients_list);

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_DBUS_CLIENT_TYPES_H_
