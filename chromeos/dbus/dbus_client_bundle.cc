// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/dbus_client_bundle.h"

#include <stddef.h>

#include <vector>

#include "base/command_line.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/arc_obb_mounter_client.h"
#include "chromeos/dbus/cras_audio_client.h"
#include "chromeos/dbus/cros_disks_client.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/debug_daemon_client.h"
#include "chromeos/dbus/easy_unlock_client.h"
#include "chromeos/dbus/fake_arc_obb_mounter_client.h"
#include "chromeos/dbus/fake_cras_audio_client.h"
#include "chromeos/dbus/fake_cryptohome_client.h"
#include "chromeos/dbus/fake_debug_daemon_client.h"
#include "chromeos/dbus/fake_easy_unlock_client.h"
#include "chromeos/dbus/fake_gsm_sms_client.h"
#include "chromeos/dbus/fake_image_burner_client.h"
#include "chromeos/dbus/fake_lorgnette_manager_client.h"
#include "chromeos/dbus/fake_modem_messaging_client.h"
#include "chromeos/dbus/fake_permission_broker_client.h"
#include "chromeos/dbus/fake_shill_device_client.h"
#include "chromeos/dbus/fake_shill_ipconfig_client.h"
#include "chromeos/dbus/fake_shill_manager_client.h"
#include "chromeos/dbus/fake_shill_profile_client.h"
#include "chromeos/dbus/fake_shill_service_client.h"
#include "chromeos/dbus/fake_shill_third_party_vpn_driver_client.h"
#include "chromeos/dbus/fake_sms_client.h"
#include "chromeos/dbus/fake_system_clock_client.h"
#include "chromeos/dbus/gsm_sms_client.h"
#include "chromeos/dbus/image_burner_client.h"
#include "chromeos/dbus/lorgnette_manager_client.h"
#include "chromeos/dbus/modem_messaging_client.h"
#include "chromeos/dbus/permission_broker_client.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/dbus/power_policy_controller.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/dbus/shill_ipconfig_client.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/dbus/shill_profile_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/dbus/shill_third_party_vpn_driver_client.h"
#include "chromeos/dbus/sms_client.h"
#include "chromeos/dbus/system_clock_client.h"
#include "chromeos/dbus/update_engine_client.h"

namespace chromeos {

DBusClientBundle::DBusClientBundle(DBusClientTypeMask real_client_mask)
    : real_client_mask_(real_client_mask) {
  if (IsUsingReal(DBusClientType::ARC_OBB_MOUNTER))
    arc_obb_mounter_client_.reset(ArcObbMounterClient::Create());
  else
    arc_obb_mounter_client_.reset(new FakeArcObbMounterClient);

  if (IsUsingReal(DBusClientType::CRAS))
    cras_audio_client_.reset(CrasAudioClient::Create());
  else
    cras_audio_client_.reset(new FakeCrasAudioClient);

  cros_disks_client_.reset(
      CrosDisksClient::Create(IsUsingReal(DBusClientType::CROS_DISKS)
                                  ? REAL_DBUS_CLIENT_IMPLEMENTATION
                                  : FAKE_DBUS_CLIENT_IMPLEMENTATION));

  if (IsUsingReal(DBusClientType::CRYPTOHOME))
    cryptohome_client_.reset(CryptohomeClient::Create());
  else
    cryptohome_client_.reset(new FakeCryptohomeClient);

  if (IsUsingReal(DBusClientType::DEBUG_DAEMON))
    debug_daemon_client_.reset(DebugDaemonClient::Create());
  else
    debug_daemon_client_.reset(new FakeDebugDaemonClient);

  if (IsUsingReal(DBusClientType::EASY_UNLOCK))
    easy_unlock_client_.reset(EasyUnlockClient::Create());
  else
    easy_unlock_client_.reset(new FakeEasyUnlockClient);

  if (IsUsingReal(DBusClientType::LORGNETTE_MANAGER))
    lorgnette_manager_client_.reset(LorgnetteManagerClient::Create());
  else
    lorgnette_manager_client_.reset(new FakeLorgnetteManagerClient);

  if (IsUsingReal(DBusClientType::SHILL)) {
    shill_manager_client_.reset(ShillManagerClient::Create());
    shill_device_client_.reset(ShillDeviceClient::Create());
    shill_ipconfig_client_.reset(ShillIPConfigClient::Create());
    shill_service_client_.reset(ShillServiceClient::Create());
    shill_profile_client_.reset(ShillProfileClient::Create());
    shill_third_party_vpn_driver_client_.reset(
        ShillThirdPartyVpnDriverClient::Create());
  } else {
    shill_manager_client_.reset(new FakeShillManagerClient);
    shill_device_client_.reset(new FakeShillDeviceClient);
    shill_ipconfig_client_.reset(new FakeShillIPConfigClient);
    shill_service_client_.reset(new FakeShillServiceClient);
    shill_profile_client_.reset(new FakeShillProfileClient);
    shill_third_party_vpn_driver_client_.reset(
        new FakeShillThirdPartyVpnDriverClient);
  }

  if (IsUsingReal(DBusClientType::GSM_SMS)) {
    gsm_sms_client_.reset(GsmSMSClient::Create());
  } else {
    FakeGsmSMSClient* gsm_sms_client = new FakeGsmSMSClient();
    gsm_sms_client->set_sms_test_message_switch_present(
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            chromeos::switches::kSmsTestMessages));
    gsm_sms_client_.reset(gsm_sms_client);
  }

  if (IsUsingReal(DBusClientType::IMAGE_BURNER))
    image_burner_client_.reset(ImageBurnerClient::Create());
  else
    image_burner_client_.reset(new FakeImageBurnerClient);

  if (IsUsingReal(DBusClientType::MODEM_MESSAGING))
    modem_messaging_client_.reset(ModemMessagingClient::Create());
  else
    modem_messaging_client_.reset(new FakeModemMessagingClient);

  if (IsUsingReal(DBusClientType::PERMISSION_BROKER))
    permission_broker_client_.reset(PermissionBrokerClient::Create());
  else
    permission_broker_client_.reset(new FakePermissionBrokerClient);

  power_manager_client_.reset(
      PowerManagerClient::Create(IsUsingReal(DBusClientType::POWER_MANAGER)
                                     ? REAL_DBUS_CLIENT_IMPLEMENTATION
                                     : FAKE_DBUS_CLIENT_IMPLEMENTATION));

  session_manager_client_.reset(
      SessionManagerClient::Create(IsUsingReal(DBusClientType::SESSION_MANAGER)
                                       ? REAL_DBUS_CLIENT_IMPLEMENTATION
                                       : FAKE_DBUS_CLIENT_IMPLEMENTATION));

  if (IsUsingReal(DBusClientType::SMS))
    sms_client_.reset(SMSClient::Create());
  else
    sms_client_.reset(new FakeSMSClient);

  if (IsUsingReal(DBusClientType::SYSTEM_CLOCK))
    system_clock_client_.reset(SystemClockClient::Create());
  else
    system_clock_client_.reset(new FakeSystemClockClient);

  update_engine_client_.reset(
      UpdateEngineClient::Create(IsUsingReal(DBusClientType::UPDATE_ENGINE)
                                     ? REAL_DBUS_CLIENT_IMPLEMENTATION
                                     : FAKE_DBUS_CLIENT_IMPLEMENTATION));
}

DBusClientBundle::~DBusClientBundle() {}

bool DBusClientBundle::IsUsingReal(DBusClientType client) const {
  return real_client_mask_ & static_cast<DBusClientTypeMask>(client);
}

bool DBusClientBundle::IsUsingAnyRealClient() const {
  return real_client_mask_ !=
         static_cast<DBusClientTypeMask>(DBusClientType::NONE);
}

void DBusClientBundle::SetupDefaultEnvironment() {
  ShillManagerClient::TestInterface* manager =
      shill_manager_client_->GetTestInterface();
  if (manager)
    manager->SetupDefaultEnvironment();
}

}  // namespace chromeos
