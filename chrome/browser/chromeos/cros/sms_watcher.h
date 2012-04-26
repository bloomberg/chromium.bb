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
class ListValue;

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

  SMSWatcher(const std::string& modem_device_path,
             MonitorSMSCallback callback,
             void* object);
  virtual ~SMSWatcher();

 private:
  // Callback for flimflam device's GetProperties() method.
  void DevicePropertiesCallback(DBusMethodCallStatus call_status,
                                const base::DictionaryValue& properties);

  // Callback for SmsReceived signal.
  void OnSmsReceived(uint32 index, bool complete);

  // Runs |callback_| with a SMS.
  void RunCallbackWithSMS(const base::DictionaryValue& sms_dictionary);

  // Callback for Get() method.
  void GetSMSCallback(uint32 index,
                      const base::DictionaryValue& sms_dictionary);

  // Callback for List() method.
  void ListSMSCallback(const base::ListValue& result);

  // Deletes SMSs in the queue.
  void DeleteSMSInChain();

  base::WeakPtrFactory<SMSWatcher> weak_ptr_factory_;
  std::string device_path_;
  MonitorSMSCallback callback_;
  void* object_;
  std::string dbus_connection_;
  dbus::ObjectPath object_path_;
  std::vector<uint32> delete_queue_;
};

}  // namespace

#endif  // CHROME_BROWSER_CHROMEOS_CROS_SMS_WATCHER_H_
