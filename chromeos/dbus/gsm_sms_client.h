// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_GSM_SMS_CLIENT_H_
#define CHROMEOS_DBUS_GSM_SMS_CLIENT_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client_implementation_type.h"

namespace base {
class DictionaryValue;
class ListValue;
}

namespace dbus {
class Bus;
class ObjectPath;
}

namespace chromeos {

// GsmSMSClient is used to communicate with
// org.freedesktop.ModemManager.Modem.Gsm.SMS service.
// All methods should be called from the origin thread (UI thread) which
// initializes the DBusThreadManager instance.
class CHROMEOS_EXPORT GsmSMSClient {
 public:
  typedef base::Callback<void(uint32 index, bool complete)> SmsReceivedHandler;
  typedef base::Callback<void()> DeleteCallback;
  typedef base::Callback<void(const base::DictionaryValue& sms)> GetCallback;
  typedef base::Callback<void(const base::ListValue& result)> ListCallback;

  virtual ~GsmSMSClient();

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static GsmSMSClient* Create(DBusClientImplementationType type,
                              dbus::Bus* bus);

  // Sets DataPlansUpdate signal handler.
  virtual void SetSmsReceivedHandler(const std::string& service_name,
                                     const dbus::ObjectPath& object_path,
                                     const SmsReceivedHandler& handler) = 0;

  // Resets DataPlansUpdate signal handler.
  virtual void ResetSmsReceivedHandler(const std::string& service_name,
                                       const dbus::ObjectPath& object_path) = 0;

  // Calls Delete method.  |callback| is called after the method call succeeds.
  virtual void Delete(const std::string& service_name,
                      const dbus::ObjectPath& object_path,
                      uint32 index,
                      const DeleteCallback& callback) = 0;

  // Calls Get method.  |callback| is called after the method call succeeds.
  virtual void Get(const std::string& service_name,
                   const dbus::ObjectPath& object_path,
                   uint32 index,
                   const GetCallback& callback) = 0;

  // Calls List method.  |callback| is called after the method call succeeds.
  virtual void List(const std::string& service_name,
                    const dbus::ObjectPath& object_path,
                    const ListCallback& callback) = 0;

  // Requests a check for new messages. In flimflam this does nothing. The
  // stub implementation uses it to generate a sequence of test messages.
  virtual void RequestUpdate(const std::string& service_name,
                             const dbus::ObjectPath& object_path) = 0;

 protected:
  // Create() should be used instead.
  GsmSMSClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(GsmSMSClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_GSM_SMS_CLIENT_H_
