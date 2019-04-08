// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SHILL_GSM_SMS_CLIENT_H_
#define CHROMEOS_DBUS_SHILL_GSM_SMS_CLIENT_H_

#include <stdint.h>

#include <string>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "chromeos/dbus/dbus_method_call_status.h"

namespace base {
class DictionaryValue;
class ListValue;
}  // namespace base

namespace dbus {
class Bus;
class ObjectPath;
}  // namespace dbus

namespace chromeos {

// GsmSMSClient is used to communicate with the
// org.freedesktop.ModemManager.Modem.Gsm.SMS service.
// All methods should be called from the origin thread (UI thread) which
// initializes the DBusThreadManager instance.
class COMPONENT_EXPORT(SHILL_CLIENT) GsmSMSClient {
 public:
  typedef base::Callback<void(uint32_t index, bool complete)>
      SmsReceivedHandler;

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates the global instance with a fake implementation.
  static void InitializeFake();

  // Destroys the global instance which must have been initialized.
  static void Shutdown();

  // Returns the global instance if initialized. May return null.
  static GsmSMSClient* Get();

  // Sets SmsReceived signal handler.
  virtual void SetSmsReceivedHandler(const std::string& service_name,
                                     const dbus::ObjectPath& object_path,
                                     const SmsReceivedHandler& handler) = 0;

  // Resets SmsReceived signal handler.
  virtual void ResetSmsReceivedHandler(const std::string& service_name,
                                       const dbus::ObjectPath& object_path) = 0;

  // Calls Delete method.  |callback| is called on method call completion.
  virtual void Delete(const std::string& service_name,
                      const dbus::ObjectPath& object_path,
                      uint32_t index,
                      VoidDBusMethodCallback callback) = 0;

  // Calls Get method.  |callback| is called on method call completion.
  virtual void Get(const std::string& service_name,
                   const dbus::ObjectPath& object_path,
                   uint32_t index,
                   DBusMethodCallback<base::DictionaryValue> callback) = 0;

  // Calls List method.  |callback| is called on method call completion.
  virtual void List(const std::string& service_name,
                    const dbus::ObjectPath& object_path,
                    DBusMethodCallback<base::ListValue> callback) = 0;

  // Requests a check for new messages. In shill this does nothing. The
  // stub implementation uses it to generate a sequence of test messages.
  virtual void RequestUpdate(const std::string& service_name,
                             const dbus::ObjectPath& object_path) = 0;

 protected:
  friend class GsmSMSClientTest;

  // Initialize/Shutdown should be used instead.
  GsmSMSClient();
  virtual ~GsmSMSClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(GsmSMSClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SHILL_GSM_SMS_CLIENT_H_
