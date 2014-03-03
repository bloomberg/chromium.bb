// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SHILL_STUB_HELPER_H_
#define CHROMEOS_DBUS_SHILL_STUB_HELPER_H_

#include <string>

#include "chromeos/chromeos_export.h"

namespace chromeos {
namespace shill_stub_helper {

// Add default devices, services and profiles. This works only if
// DBusThreadManager was initialized with the FakeShill*Clients.
void SetupDefaultEnvironment();

}  // namespace shill_stub_helper
}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SHILL_STUB_HELPER_H_
