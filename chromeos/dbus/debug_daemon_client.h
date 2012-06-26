// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_DEBUG_DAEMON_CLIENT_H_
#define CHROMEOS_DBUS_DEBUG_DAEMON_CLIENT_H_

#include "base/callback.h"
#include "base/platform_file.h"
#include "base/memory/ref_counted_memory.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client_implementation_type.h"

#include <map>

namespace dbus {
class Bus;
}  // namespace dbus

namespace chromeos {

// DebugDaemonClient is used to communicate with the debug daemon.
class CHROMEOS_EXPORT DebugDaemonClient {
 public:
  virtual ~DebugDaemonClient();

  // Called once GetDebugLogs() is complete. Takes one parameter:
  // - succeeded: was the logs stored successfully.
  typedef base::Callback<void(bool succeeded)> GetDebugLogsCallback;

  // Requests to store debug logs into |file| and calls |callback|
  // when completed. Debug logs will be stored in the .tgz format.
  virtual void GetDebugLogs(base::PlatformFile file,
                            const GetDebugLogsCallback& callback) = 0;

  // Called once SetDebugMode() is complete. Takes one parameter:
  // - succeeded: debug mode was changed successfully.
  typedef base::Callback<void(bool succeeded)> SetDebugModeCallback;

  // Requests to change debug mode to given |subsystem| and calls
  // |callback| when completed. |subsystem| should be one of the
  // following: "wifi", "ethernet", "cellular" or "none".
  virtual void SetDebugMode(const std::string& subsystem,
                            const SetDebugModeCallback& callback) = 0;

  typedef base::Callback<void(bool succeeded,
                              const std::vector<std::string>& routes)>
      GetRoutesCallback;
  virtual void GetRoutes(bool numeric, bool ipv6,
                         const GetRoutesCallback& callback) = 0;

  typedef base::Callback<void(bool succeeded, const std::string& status)>
      GetNetworkStatusCallback;

  virtual void GetNetworkStatus(const GetNetworkStatusCallback& callback) = 0;

  typedef base::Callback<void(bool succeeded, const std::string& status)>
      GetModemStatusCallback;

  virtual void GetModemStatus(const GetModemStatusCallback& callback) = 0;

  typedef base::Callback<void(bool succeeded,
                              const std::map<std::string, std::string>& logs)>
      GetAllLogsCallback;
  virtual void GetAllLogs(const GetAllLogsCallback& callback) = 0;

  // Requests to start system/kernel tracing.
  virtual void StartSystemTracing() = 0;

  // Called once RequestStopSystemTracing() is complete. Takes one parameter:
  // - result: the data collected while tracing was active
  typedef base::Callback<void(const scoped_refptr<base::RefCountedString>&
      result)> StopSystemTracingCallback;

  // Requests to stop system tracing and calls |callback| when completed.
  virtual bool RequestStopSystemTracing(const StopSystemTracingCallback&
      callback) = 0;

  // Returns an empty SystemTracingCallback that does nothing.
  static StopSystemTracingCallback EmptyStopSystemTracingCallback();

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static DebugDaemonClient* Create(DBusClientImplementationType type,
                                   dbus::Bus* bus);
 protected:
  // Create() should be used instead.
  DebugDaemonClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(DebugDaemonClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_DEBUG_DAEMON_CLIENT_H_
