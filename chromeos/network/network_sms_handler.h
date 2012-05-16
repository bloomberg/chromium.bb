// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_SMS_HANDLER_H_
#define CHROMEOS_NETWORK_NETWORK_SMS_HANDLER_H_

#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_method_call_status.h"

namespace chromeos {

// Class to watch sms without Libcros.
class CHROMEOS_EXPORT NetworkSmsHandler {
 public:
  static const char kNumberKey[];
  static const char kTextKey[];
  static const char kTimestampKey[];

  class Observer {
   public:
    virtual ~Observer() {}

    // Called when a new message arrives. |message| contains the message.
    // The contents of the dictionary include the keys listed above.
    virtual void MessageReceived(const base::DictionaryValue& message) = 0;
  };

  NetworkSmsHandler();
  ~NetworkSmsHandler();

  // Requests the devices from the netowork manager, sets up observers, and
  // requests the initial list of messages. Any observers that wish to be
  // notified with initial messages should be added before calling this.
  void Init();

  // Requests an immediate check for new messages.
  void RequestUpdate();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  class NetworkSmsDeviceHandler;

  // Called from NetworkSmsDeviceHandler when a message is received.
  void NotifyMessageReceived(const base::DictionaryValue& message);

  // Callback to handle the manager properties with the list of devices.
  void ManagerPropertiesCallback(DBusMethodCallStatus call_status,
                                 const base::DictionaryValue& properties);

  // Callback to handle the device properties for |device_path|.
  // A NetworkSmsDeviceHandler will be instantiated for each cellular device.
  void DevicePropertiesCallback(const std::string& device_path,
                                DBusMethodCallStatus call_status,
                                const base::DictionaryValue& properties);

  ObserverList<Observer> observers_;
  ScopedVector<NetworkSmsDeviceHandler> device_handlers_;
  base::WeakPtrFactory<NetworkSmsHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetworkSmsHandler);
};

}  // namespace

#endif  // CHROMEOS_NETWORK_NETWORK_SMS_HANDLER_H_
