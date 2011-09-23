// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DBUS_SENSORS_SOURCE_H_
#define CHROME_BROWSER_CHROMEOS_DBUS_SENSORS_SOURCE_H_

#include "base/memory/ref_counted.h"
#include "content/browser/browser_thread.h"

#include <string>

namespace dbus {
class Bus;
class ObjectProxy;
class Signal;
}  // namespace

namespace chromeos {

// Creates and manages a dbus signal receiver for device orientation changes,
// and eventually receives data for other motion-related sensors.
class SensorsSource
    : public base::RefCountedThreadSafe<SensorsSource> {
 public:
  // Creates an inactive sensors source.
  SensorsSource();

  // Initializes this sensor source and begins listening for signals.
  // Returns true on success.
  void Init(dbus::Bus* bus);

 private:
  friend class base::RefCountedThreadSafe<SensorsSource>;
  virtual ~SensorsSource();

  // Called by dbus:: in when an orientation change signal is received.
  void OrientationChangedReceived(dbus::Signal* signal);

  // Called by dbus:: when the orientation change signal is initially connected.
  void OrientationChangedConnected(const std::string& interface_name,
                                   const std::string& signal_name,
                                   bool success);

  dbus::ObjectProxy* sensors_proxy_;

  DISALLOW_COPY_AND_ASSIGN(SensorsSource);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DBUS_SENSORS_SOURCE_H_
