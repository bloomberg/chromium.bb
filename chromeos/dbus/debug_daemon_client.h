// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_DEBUG_DAEMON_CLIENT_H_
#define CHROMEOS_DBUS_DEBUG_DAEMON_CLIENT_H_

#include "base/callback.h"
#include "base/memory/ref_counted_memory.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client_implementation_type.h"

namespace dbus {
class Bus;
}  // namespace dbus

namespace chromeos {

// DebugDaemonClient is used to communicate with the debug daemon.
class CHROMEOS_EXPORT DebugDaemonClient {
 public:
  virtual ~DebugDaemonClient();

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
