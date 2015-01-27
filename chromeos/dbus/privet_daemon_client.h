// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_PRIVET_DAEMON_CLIENT_H_
#define CHROMEOS_DBUS_PRIVET_DAEMON_CLIENT_H_

#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client.h"

// TODO(jamescook): The DBus interface is still being defined by the privetd
// team. Move to third_party/cros_system_api/dbus/service_constants.h when the
// interface is finalized.
namespace privetd {
const char kPrivetdServiceName[] = "org.chromium.privetd";
const char kPrivetdManagerServicePath[] = "/org/chromium/privetd/Manager";
const char kPrivetdManagerInterface[] = "org.chromium.privetd.Manager";
// Methods.
const char kPingMethod[] = "Ping";
}  // namespace privetd

namespace chromeos {

// PrivetDaemonClient is used to communicate with privetd for Wi-Fi
// bootstrapping and setup. The most common user for this code would be an
// embedded Chrome OS Core device.
class CHROMEOS_EXPORT PrivetDaemonClient : public DBusClient {
 public:
  ~PrivetDaemonClient() override;

  // Returns true if privetd correctly responded to Ping().
  using PingCallback = base::Callback<void(bool success)>;

  // Asynchronously pings the daemon.
  virtual void Ping(const PingCallback& callback) = 0;

  // Creates a new instance and returns ownership.
  static PrivetDaemonClient* Create();

 protected:
  // Create() should be used instead.
  PrivetDaemonClient();

 private:
  friend class PrivetDaemonClientTest;

  DISALLOW_COPY_AND_ASSIGN(PrivetDaemonClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_PRIVET_DAEMON_CLIENT_H_
