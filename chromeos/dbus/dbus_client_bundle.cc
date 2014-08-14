// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/dbus_client_bundle.h"

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chromeos/dbus/bluetooth_adapter_client.h"
#include "chromeos/dbus/bluetooth_agent_manager_client.h"
#include "chromeos/dbus/bluetooth_device_client.h"
#include "chromeos/dbus/bluetooth_gatt_characteristic_client.h"
#include "chromeos/dbus/bluetooth_gatt_descriptor_client.h"
#include "chromeos/dbus/bluetooth_gatt_manager_client.h"
#include "chromeos/dbus/bluetooth_gatt_service_client.h"
#include "chromeos/dbus/bluetooth_input_client.h"
#include "chromeos/dbus/bluetooth_profile_manager_client.h"
#include "chromeos/dbus/cras_audio_client.h"
#include "chromeos/dbus/cros_disks_client.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon_client.h"
#include "chromeos/dbus/easy_unlock_client.h"
#include "chromeos/dbus/gsm_sms_client.h"
#include "chromeos/dbus/image_burner_client.h"
#include "chromeos/dbus/introspectable_client.h"
#include "chromeos/dbus/lorgnette_manager_client.h"
#include "chromeos/dbus/modem_messaging_client.h"
#include "chromeos/dbus/nfc_adapter_client.h"
#include "chromeos/dbus/nfc_device_client.h"
#include "chromeos/dbus/nfc_manager_client.h"
#include "chromeos/dbus/nfc_record_client.h"
#include "chromeos/dbus/nfc_tag_client.h"
#include "chromeos/dbus/permission_broker_client.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/dbus/power_policy_controller.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/dbus/shill_ipconfig_client.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/dbus/shill_profile_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/dbus/sms_client.h"
#include "chromeos/dbus/system_clock_client.h"
#include "chromeos/dbus/update_engine_client.h"

namespace chromeos {

namespace {

// Command line switch mapping for --dbus-unstub-clients.
const struct {
  const char* param_name;
  DBusClientBundle::DBusClientType client_type;
} client_type_map[] = {
    { "bluetooth",  DBusClientBundle::BLUETOOTH },
    { "cras",  DBusClientBundle::CRAS },
    { "cros_disks",  DBusClientBundle::CROS_DISKS },
    { "cryptohome",  DBusClientBundle::CRYPTOHOME },
    { "debug_daemon",  DBusClientBundle::DEBUG_DAEMON },
    { "easy_unlock",  DBusClientBundle::EASY_UNLOCK },
    { "lorgnette_manager",  DBusClientBundle::LORGNETTE_MANAGER },
    { "shill",  DBusClientBundle::SHILL },
    { "gsm_sms",  DBusClientBundle::GSM_SMS },
    { "image_burner",  DBusClientBundle::IMAGE_BURNER },
    { "introspectable",  DBusClientBundle::INTROSPECTABLE },
    { "modem_messaging",  DBusClientBundle::MODEM_MESSAGING },
    { "nfc",  DBusClientBundle::NFC },
    { "permission_broker",  DBusClientBundle::PERMISSION_BROKER },
    { "power_manager",  DBusClientBundle::POWER_MANAGER },
    { "session_manager",  DBusClientBundle::SESSION_MANAGER },
    { "sms",  DBusClientBundle::SMS },
    { "system_clock",  DBusClientBundle::SYSTEM_CLOCK },
    { "update_engine",  DBusClientBundle::UPDATE_ENGINE },
};

// Parses single command line param value for dbus subsystem and returns its
// enum representation. DBusClientType::UNKWNOWN is returned if |client_type|
// does not match any known dbus client.
DBusClientBundle::DBusClientType GetDBusClientType(
    const std::string& client_type) {
  for (size_t i = 0; i < arraysize(client_type_map); i++) {
    if (LowerCaseEqualsASCII(client_type, client_type_map[i].param_name))
      return client_type_map[i].client_type;
  }
  return DBusClientBundle::NO_CLIENTS;
}

}  // namespace

DBusClientBundle::DBusClientBundle() {
  const DBusClientImplementationType type = REAL_DBUS_CLIENT_IMPLEMENTATION;

  if (!DBusThreadManager::IsUsingStub(BLUETOOTH)) {
    bluetooth_adapter_client_.reset(BluetoothAdapterClient::Create());
    bluetooth_agent_manager_client_.reset(
        BluetoothAgentManagerClient::Create());
    bluetooth_device_client_.reset(BluetoothDeviceClient::Create());
    bluetooth_input_client_.reset(BluetoothInputClient::Create());
    bluetooth_profile_manager_client_.reset(
        BluetoothProfileManagerClient::Create());
    bluetooth_gatt_characteristic_client_.reset(
        BluetoothGattCharacteristicClient::Create());
    bluetooth_gatt_descriptor_client_.reset(
        BluetoothGattDescriptorClient::Create());
    bluetooth_gatt_manager_client_.reset(
        BluetoothGattManagerClient::Create());
    bluetooth_gatt_service_client_.reset(
        BluetoothGattServiceClient::Create());
  }

  if (!DBusThreadManager::IsUsingStub(CRAS))
    cras_audio_client_.reset(CrasAudioClient::Create());

  if (!DBusThreadManager::IsUsingStub(CROS_DISKS))
    cros_disks_client_.reset(CrosDisksClient::Create(type));

  if (!DBusThreadManager::IsUsingStub(CRYPTOHOME))
    cryptohome_client_.reset(CryptohomeClient::Create());

  if (!DBusThreadManager::IsUsingStub(DEBUG_DAEMON))
    debug_daemon_client_.reset(DebugDaemonClient::Create());

  if (!DBusThreadManager::IsUsingStub(EASY_UNLOCK))
    easy_unlock_client_.reset(EasyUnlockClient::Create());

  if (!DBusThreadManager::IsUsingStub(LORGNETTE_MANAGER))
    lorgnette_manager_client_.reset(LorgnetteManagerClient::Create());

  if (!DBusThreadManager::IsUsingStub(SHILL)) {
    shill_manager_client_.reset(ShillManagerClient::Create());
    shill_device_client_.reset(ShillDeviceClient::Create());
    shill_ipconfig_client_.reset(ShillIPConfigClient::Create());
    shill_service_client_.reset(ShillServiceClient::Create());
    shill_profile_client_.reset(ShillProfileClient::Create());
  }

  if (!DBusThreadManager::IsUsingStub(GSM_SMS))
    gsm_sms_client_.reset(GsmSMSClient::Create());

  if (!DBusThreadManager::IsUsingStub(IMAGE_BURNER))
    image_burner_client_.reset(ImageBurnerClient::Create());

  if (!DBusThreadManager::IsUsingStub(INTROSPECTABLE))
    introspectable_client_.reset(IntrospectableClient::Create());

  if (!DBusThreadManager::IsUsingStub(MODEM_MESSAGING))
    modem_messaging_client_.reset(ModemMessagingClient::Create());

  // Create the NFC clients in the correct order based on their dependencies.
  if (!DBusThreadManager::IsUsingStub(NFC)) {
    nfc_manager_client_.reset(NfcManagerClient::Create());
    nfc_adapter_client_.reset(
        NfcAdapterClient::Create(nfc_manager_client_.get()));
    nfc_device_client_.reset(
        NfcDeviceClient::Create(nfc_adapter_client_.get()));
    nfc_tag_client_.reset(NfcTagClient::Create(nfc_adapter_client_.get()));
    nfc_record_client_.reset(NfcRecordClient::Create(nfc_device_client_.get(),
                                                     nfc_tag_client_.get()));
  }

  if (!DBusThreadManager::IsUsingStub(PERMISSION_BROKER))
    permission_broker_client_.reset(PermissionBrokerClient::Create());

  if (!DBusThreadManager::IsUsingStub(POWER_MANAGER))
    power_manager_client_.reset(PowerManagerClient::Create(type));

  if (!DBusThreadManager::IsUsingStub(SESSION_MANAGER))
    session_manager_client_.reset(SessionManagerClient::Create(type));

  if (!DBusThreadManager::IsUsingStub(SMS))
    sms_client_.reset(SMSClient::Create());

  if (!DBusThreadManager::IsUsingStub(SYSTEM_CLOCK))
    system_clock_client_.reset(SystemClockClient::Create());

  if (!DBusThreadManager::IsUsingStub(UPDATE_ENGINE))
    update_engine_client_.reset(UpdateEngineClient::Create(type));
}

DBusClientBundle::~DBusClientBundle() {
}

// static
DBusClientBundle::DBusClientTypeMask DBusClientBundle::ParseUnstubList(
    const std::string& unstub_list) {
  DBusClientTypeMask unstub_mask = 0;
  std::vector<std::string> unstub_components;
  base::SplitString(unstub_list, ',', &unstub_components);
  for (std::vector<std::string>::const_iterator iter =
          unstub_components.begin();
       iter != unstub_components.end(); ++iter) {
    DBusClientBundle::DBusClientType client = GetDBusClientType(*iter);
    if (client != DBusClientBundle::NO_CLIENTS) {
      LOG(WARNING) << "Unstubbing dbus client for " << *iter;
      unstub_mask |= client;
    } else {
      LOG(ERROR) << "Unknown dbus client: " << *iter;
    }
  }

  return unstub_mask;
}

}  // namespace chromeos
