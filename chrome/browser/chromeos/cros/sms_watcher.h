// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_SMS_WATCHER_H_
#define CHROME_BROWSER_CHROMEOS_CROS_SMS_WATCHER_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/cros/cros_network_functions.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "dbus/object_path.h"

namespace base {

class DictionaryValue;

}  // namespace base

namespace chromeos {

// Class to watch sms without Libcros.
class SMSWatcher : public CrosNetworkWatcher {
 public:
  // Dictionary key constants.
  static const char kNumberKey[];
  static const char kTextKey[];
  static const char kTimestampKey[];
  static const char kSmscKey[];
  static const char kValidityKey[];
  static const char kClassKey[];
  static const char kIndexKey[];

  static const char kModemManager1NumberKey[];
  static const char kModemManager1TextKey[];
  static const char kModemManager1TimestampKey[];
  static const char kModemManager1SmscKey[];
  static const char kModemManager1ValidityKey[];
  static const char kModemManager1ClassKey[];
  static const char kModemManager1IndexKey[];

  // Base class of watcher implementation classes.  Public to allow
  // derived classes in the anonymous namespace to inherit from it.
  class WatcherBase;

  SMSWatcher(const std::string& modem_device_path,
             MonitorSMSCallback callback);
  virtual ~SMSWatcher();

 private:
  // Callback for shill device's GetProperties() method.
  void DevicePropertiesCallback(DBusMethodCallStatus call_status,
                                const base::DictionaryValue& properties);

  base::WeakPtrFactory<SMSWatcher> weak_ptr_factory_;
  std::string device_path_;
  MonitorSMSCallback callback_;
  scoped_ptr<WatcherBase> watcher_;

  DISALLOW_COPY_AND_ASSIGN(SMSWatcher);
};

}  // namespace

#endif  // CHROME_BROWSER_CHROMEOS_CROS_SMS_WATCHER_H_
