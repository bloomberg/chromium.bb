// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DBUS_SENSORS_CLIENT_H_
#define CHROME_BROWSER_CHROMEOS_DBUS_SENSORS_CLIENT_H_

#include "base/basictypes.h"

namespace dbus {
class Bus;
}  // namespace

namespace chromeos {

// Creates and manages a dbus signal receiver for device orientation changes,
// and eventually receives data for other motion-related sensors.
class SensorsClient {
 public:
  virtual ~SensorsClient();

  // Creates the instance.
  static SensorsClient* Create(dbus::Bus* bus);

 protected:
  // Create() should be used instead.
  SensorsClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(SensorsClient);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DBUS_SENSORS_CLIENT_H_
