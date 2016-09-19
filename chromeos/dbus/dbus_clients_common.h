// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_DBUS_CLIENTS_COMMON_H_
#define CHROMEOS_DBUS_DBUS_CLIENTS_COMMON_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client_types.h"

namespace dbus {
class Bus;
}

namespace chromeos {

class CrasAudioClient;
class CryptohomeClient;
class GsmSMSClient;
class ModemMessagingClient;
class PermissionBrokerClient;
class PowerManagerClient;
class SessionManagerClient;
class ShillDeviceClient;
class ShillIPConfigClient;
class ShillManagerClient;
class ShillProfileClient;
class ShillServiceClient;
class ShillThirdPartyVpnDriverClient;
class SMSClient;
class SystemClockClient;
class UpdateEngineClient;

// D-Bus clients used in multiple processes (e.g. ash, browser, mus).
class CHROMEOS_EXPORT DBusClientsCommon {
 public:
  // Creates real implementations for |real_client_mask| and fakes for all
  // others. Fakes are used when running on Linux desktop and in tests.
  explicit DBusClientsCommon(DBusClientTypeMask real_client_mask);
  ~DBusClientsCommon();

  // Returns true if |client| has a real (non-fake) client implementation.
  bool IsUsingReal(DBusClientType client) const;

  // Initialize proper runtime environment for its dbus clients.
  void Initialize(dbus::Bus* system_bus);

 private:
  friend class DBusThreadManager;
  friend class DBusThreadManagerSetter;

  // Bitmask for clients with real implementations.
  const DBusClientTypeMask real_client_mask_;

  std::unique_ptr<CrasAudioClient> cras_audio_client_;
  std::unique_ptr<CryptohomeClient> cryptohome_client_;
  std::unique_ptr<GsmSMSClient> gsm_sms_client_;
  std::unique_ptr<ModemMessagingClient> modem_messaging_client_;
  std::unique_ptr<ShillDeviceClient> shill_device_client_;
  std::unique_ptr<ShillIPConfigClient> shill_ipconfig_client_;
  std::unique_ptr<ShillManagerClient> shill_manager_client_;
  std::unique_ptr<ShillServiceClient> shill_service_client_;
  std::unique_ptr<ShillProfileClient> shill_profile_client_;
  std::unique_ptr<ShillThirdPartyVpnDriverClient>
      shill_third_party_vpn_driver_client_;
  std::unique_ptr<PermissionBrokerClient> permission_broker_client_;
  std::unique_ptr<SMSClient> sms_client_;
  std::unique_ptr<SystemClockClient> system_clock_client_;
  std::unique_ptr<PowerManagerClient> power_manager_client_;
  std::unique_ptr<SessionManagerClient> session_manager_client_;
  std::unique_ptr<UpdateEngineClient> update_engine_client_;

  DISALLOW_COPY_AND_ASSIGN(DBusClientsCommon);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_DBUS_CLIENTS_COMMON_H_
