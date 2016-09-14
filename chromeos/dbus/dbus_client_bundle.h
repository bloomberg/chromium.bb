// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_DBUS_CLIENT_BUNDLE_H_
#define CHROMEOS_DBUS_DBUS_CLIENT_BUNDLE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client_types.h"

namespace chromeos {

class ArcObbMounterClient;
class CrasAudioClient;
class CrosDisksClient;
class CryptohomeClient;
class DebugDaemonClient;
class EasyUnlockClient;
class GsmSMSClient;
class ImageBurnerClient;
class LorgnetteManagerClient;
class ModemMessagingClient;
class PermissionBrokerClient;
class PowerManagerClient;
class SMSClient;
class SessionManagerClient;
class ShillDeviceClient;
class ShillIPConfigClient;
class ShillManagerClient;
class ShillProfileClient;
class ShillServiceClient;
class ShillThirdPartyVpnDriverClient;
class SystemClockClient;
class UpdateEngineClient;

// The bundle of all D-Bus clients used in DBusThreadManager. The bundle
// is used to delete them at once in the right order before shutting down the
// system bus. See also the comment in the destructor of DBusThreadManager.
class CHROMEOS_EXPORT DBusClientBundle {
 public:
  // Creates real implementations for |real_client_mask| and fakes for all
  // others. Fakes are used when running on Linux desktop and in tests.
  explicit DBusClientBundle(DBusClientTypeMask real_client_mask);
  ~DBusClientBundle();

  // Returns true if |client| has a real (non-fake) client implementation.
  bool IsUsingReal(DBusClientType client) const;

  // Returns true if any real DBusClient is used.
  bool IsUsingAnyRealClient() const;

  // Initialize proper runtime environment for its dbus clients.
  void SetupDefaultEnvironment();

  ArcObbMounterClient* arc_obb_mounter_client() {
    return arc_obb_mounter_client_.get();
  }

  CrasAudioClient* cras_audio_client() {
    return cras_audio_client_.get();
  }

  CrosDisksClient* cros_disks_client() {
    return cros_disks_client_.get();
  }

  CryptohomeClient* cryptohome_client() {
    return cryptohome_client_.get();
  }

  DebugDaemonClient* debug_daemon_client() {
    return debug_daemon_client_.get();
  }

  EasyUnlockClient* easy_unlock_client() {
    return easy_unlock_client_.get();
  }

  LorgnetteManagerClient* lorgnette_manager_client() {
    return lorgnette_manager_client_.get();
  }

  ShillDeviceClient* shill_device_client() {
    return shill_device_client_.get();
  }

  ShillIPConfigClient* shill_ipconfig_client() {
    return shill_ipconfig_client_.get();
  }

  ShillManagerClient* shill_manager_client() {
    return shill_manager_client_.get();
  }

  ShillServiceClient* shill_service_client() {
    return shill_service_client_.get();
  }

  ShillProfileClient* shill_profile_client() {
    return shill_profile_client_.get();
  }

  ShillThirdPartyVpnDriverClient* shill_third_party_vpn_driver_client() {
    return shill_third_party_vpn_driver_client_.get();
  }

  GsmSMSClient* gsm_sms_client() {
    return gsm_sms_client_.get();
  }

  ImageBurnerClient* image_burner_client() {
    return image_burner_client_.get();
  }

  ModemMessagingClient* modem_messaging_client() {
    return modem_messaging_client_.get();
  }

  PermissionBrokerClient* permission_broker_client() {
    return permission_broker_client_.get();
  }

  SystemClockClient* system_clock_client() {
    return system_clock_client_.get();
  }

  PowerManagerClient* power_manager_client() {
    return power_manager_client_.get();
  }

  SessionManagerClient* session_manager_client() {
    return session_manager_client_.get();
  }

  SMSClient* sms_client() {
    return sms_client_.get();
  }

  UpdateEngineClient* update_engine_client() {
    return update_engine_client_.get();
  }

 private:
  friend class DBusThreadManagerSetter;

  // Bitmask for clients with real implementations.
  const DBusClientTypeMask real_client_mask_;

  std::unique_ptr<ArcObbMounterClient> arc_obb_mounter_client_;
  std::unique_ptr<CrasAudioClient> cras_audio_client_;
  std::unique_ptr<CrosDisksClient> cros_disks_client_;
  std::unique_ptr<CryptohomeClient> cryptohome_client_;
  std::unique_ptr<DebugDaemonClient> debug_daemon_client_;
  std::unique_ptr<EasyUnlockClient> easy_unlock_client_;
  std::unique_ptr<LorgnetteManagerClient> lorgnette_manager_client_;
  std::unique_ptr<ShillDeviceClient> shill_device_client_;
  std::unique_ptr<ShillIPConfigClient> shill_ipconfig_client_;
  std::unique_ptr<ShillManagerClient> shill_manager_client_;
  std::unique_ptr<ShillServiceClient> shill_service_client_;
  std::unique_ptr<ShillProfileClient> shill_profile_client_;
  std::unique_ptr<ShillThirdPartyVpnDriverClient>
      shill_third_party_vpn_driver_client_;
  std::unique_ptr<GsmSMSClient> gsm_sms_client_;
  std::unique_ptr<ImageBurnerClient> image_burner_client_;
  std::unique_ptr<ModemMessagingClient> modem_messaging_client_;
  std::unique_ptr<PermissionBrokerClient> permission_broker_client_;
  std::unique_ptr<SystemClockClient> system_clock_client_;
  std::unique_ptr<PowerManagerClient> power_manager_client_;
  std::unique_ptr<SessionManagerClient> session_manager_client_;
  std::unique_ptr<SMSClient> sms_client_;
  std::unique_ptr<UpdateEngineClient> update_engine_client_;

  DISALLOW_COPY_AND_ASSIGN(DBusClientBundle);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_DBUS_CLIENT_BUNDLE_H_
