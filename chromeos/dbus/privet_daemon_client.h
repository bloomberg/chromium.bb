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
const char kPrivetdInterface[] = "org.chromium.privetd";
const char kPrivetdServicePath[] = "/org/chromium/privetd";
const char kPrivetdServiceName[] = "org.chromium.privetd";

// Signals.
const char kSetupStatusChangedSignal[] = "SetupStatusChanged";

// Setup status values.
const char kSetupCompleted[] = "completed";
}  // namespace privetd

namespace chromeos {

// PrivetDaemonClient is used to communicate with privetd for Wi-Fi
// bootstrapping and setup. The most common user for this code would be an
// embedded Chrome OS Core device.
class CHROMEOS_EXPORT PrivetDaemonClient : public DBusClient {
 public:
  ~PrivetDaemonClient() override;

  // Called with setup status.
  using GetSetupStatusCallback =
      base::Callback<void(const std::string& status)>;

  // Asynchronously gets the current setup status. The setup status strings can
  // be found in third_party/cros_system_api/dbus/service_constants.h.
  virtual void GetSetupStatus(const GetSetupStatusCallback& callback) = 0;

  // Creates a new instance and returns ownership.
  static PrivetDaemonClient* Create();

 protected:
  // Create() should be used instead.
  PrivetDaemonClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(PrivetDaemonClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_PRIVET_DAEMON_CLIENT_H_
