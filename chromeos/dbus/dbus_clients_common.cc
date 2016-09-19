// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/dbus_clients_common.h"

#include "base/command_line.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/cras_audio_client.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_cras_audio_client.h"
#include "chromeos/dbus/fake_cryptohome_client.h"
#include "chromeos/dbus/fake_gsm_sms_client.h"
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

DBusClientsCommon::DBusClientsCommon(DBusClientTypeMask real_client_mask)
    : real_client_mask_(real_client_mask) {
  if (IsUsingReal(DBusClientType::CRAS))
    cras_audio_client_.reset(CrasAudioClient::Create());
  else
    cras_audio_client_.reset(new FakeCrasAudioClient);

  if (IsUsingReal(DBusClientType::CRYPTOHOME))
    cryptohome_client_.reset(CryptohomeClient::Create());
  else
    cryptohome_client_.reset(new FakeCryptohomeClient);

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

DBusClientsCommon::~DBusClientsCommon() {}

bool DBusClientsCommon::IsUsingReal(DBusClientType client) const {
  return real_client_mask_ & static_cast<DBusClientTypeMask>(client);
}

void DBusClientsCommon::Initialize(dbus::Bus* system_bus) {
  DCHECK(DBusThreadManager::IsInitialized());

  cras_audio_client_->Init(system_bus);
  cryptohome_client_->Init(system_bus);
  gsm_sms_client_->Init(system_bus);
  modem_messaging_client_->Init(system_bus);
  permission_broker_client_->Init(system_bus);
  power_manager_client_->Init(system_bus);
  session_manager_client_->Init(system_bus);
  shill_device_client_->Init(system_bus);
  shill_ipconfig_client_->Init(system_bus);
  shill_manager_client_->Init(system_bus);
  shill_service_client_->Init(system_bus);
  shill_profile_client_->Init(system_bus);
  shill_third_party_vpn_driver_client_->Init(system_bus);
  sms_client_->Init(system_bus);
  system_clock_client_->Init(system_bus);
  update_engine_client_->Init(system_bus);

  ShillManagerClient::TestInterface* manager =
      shill_manager_client_->GetTestInterface();
  if (manager)
    manager->SetupDefaultEnvironment();
}

}  // namespace chromeos
