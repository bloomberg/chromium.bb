// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SENSORS_SOURCE_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_SENSORS_SOURCE_CHROMEOS_H_

#include "base/memory/ref_counted.h"
#include "content/browser/browser_thread.h"

#include <string>

namespace dbus {
class Bus;
class ObjectProxy;
class Signal;
}  // namespace

namespace sensors {

// Creates and manages a dbus signal receiver for device orientation changes,
// and eventually receives data for other motion-related sensors.
class SensorsSourceChromeos
    : public base::RefCountedThreadSafe<SensorsSourceChromeos,
                                        BrowserThread::DeleteOnUIThread> {
 public:
  // Creates an inactive sensors source.
  SensorsSourceChromeos();

  // Initializes this sensor source and begins listening for signals.
  // Returns true on success.
  //
  // The sensor source performs DBus operations using BrowserThread::FILE,
  // which must have been started before calling this method.
  bool Init();

  // Shuts down this sensors source and the underlying DBus connection.
  void Stop();

 private:
  friend struct BrowserThread::DeleteOnThread<BrowserThread::UI>;
  friend class DeleteTask<SensorsSourceChromeos>;
  virtual ~SensorsSourceChromeos();

  // Called by dbus:: in when an orientation change signal is received.
  void OrientationChangedReceived(dbus::Signal* signal);

  // Called by dbus:: when the orientation change signal is initially connected.
  void OrientationChangedConnected(const std::string& interface_name,
                                   const std::string& signal_name,
                                   bool success);

  scoped_refptr<dbus::Bus> bus_;
  dbus::ObjectProxy* sensors_proxy_;

  DISALLOW_COPY_AND_ASSIGN(SensorsSourceChromeos);
};

}  // namespace sensors

#endif  // CHROME_BROWSER_CHROMEOS_SENSORS_SOURCE_CHROMEOS_H_
