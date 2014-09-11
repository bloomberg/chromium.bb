// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/dbus_thread_manager.h"

#include "base/command_line.h"
#include "base/sys_info.h"
#include "base/threading/thread.h"
#include "chromeos/chromeos_switches.h"
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
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/dbus/dbus_client_bundle.h"
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
#include "dbus/bus.h"
#include "dbus/dbus_statistics.h"

namespace chromeos {

static DBusThreadManager* g_dbus_thread_manager = NULL;
static bool g_using_dbus_thread_manager_for_testing = false;

DBusThreadManager::DBusThreadManager(scoped_ptr<DBusClientBundle> client_bundle)
    : client_bundle_(client_bundle.Pass()) {
  dbus::statistics::Initialize();

  if (client_bundle_->IsUsingAnyRealClient()) {
    // At least one real DBusClient is used.
    // Create the D-Bus thread.
    base::Thread::Options thread_options;
    thread_options.message_loop_type = base::MessageLoop::TYPE_IO;
    dbus_thread_.reset(new base::Thread("D-Bus thread"));
    dbus_thread_->StartWithOptions(thread_options);

    // Create the connection to the system bus.
    dbus::Bus::Options system_bus_options;
    system_bus_options.bus_type = dbus::Bus::SYSTEM;
    system_bus_options.connection_type = dbus::Bus::PRIVATE;
    system_bus_options.dbus_task_runner = dbus_thread_->message_loop_proxy();
    system_bus_ = new dbus::Bus(system_bus_options);
  }

  // TODO(crbug.com/345586): Move PowerPolicyController out of
  // DBusThreadManager.
  power_policy_controller_.reset(new PowerPolicyController);
}

DBusThreadManager::~DBusThreadManager() {
  // PowerPolicyController's destructor depends on PowerManagerClient.
  power_policy_controller_.reset();

  // Delete all D-Bus clients before shutting down the system bus.
  client_bundle_.reset();

  // Shut down the bus. During the browser shutdown, it's ok to shut down
  // the bus synchronously.
  if (system_bus_.get())
    system_bus_->ShutdownOnDBusThreadAndBlock();

  // Stop the D-Bus thread.
  if (dbus_thread_)
    dbus_thread_->Stop();

  dbus::statistics::Shutdown();

  if (!g_dbus_thread_manager)
    return;  // Called form Shutdown() or local test instance.

  // There should never be both a global instance and a local instance.
  CHECK(this == g_dbus_thread_manager);
  if (g_using_dbus_thread_manager_for_testing) {
    g_dbus_thread_manager = NULL;
    g_using_dbus_thread_manager_for_testing = false;
    VLOG(1) << "DBusThreadManager destroyed";
  } else {
    LOG(FATAL) << "~DBusThreadManager() called outside of Shutdown()";
  }
}

dbus::Bus* DBusThreadManager::GetSystemBus() {
  return system_bus_.get();
}

BluetoothAdapterClient* DBusThreadManager::GetBluetoothAdapterClient() {
  return client_bundle_->bluetooth_adapter_client();
}

BluetoothAgentManagerClient*
DBusThreadManager::GetBluetoothAgentManagerClient() {
  return client_bundle_->bluetooth_agent_manager_client();
}

BluetoothDeviceClient* DBusThreadManager::GetBluetoothDeviceClient() {
  return client_bundle_->bluetooth_device_client();
}

BluetoothGattCharacteristicClient*
DBusThreadManager::GetBluetoothGattCharacteristicClient() {
  return client_bundle_->bluetooth_gatt_characteristic_client();
}

BluetoothGattDescriptorClient*
DBusThreadManager::GetBluetoothGattDescriptorClient() {
  return client_bundle_->bluetooth_gatt_descriptor_client();
}

BluetoothGattManagerClient*
DBusThreadManager::GetBluetoothGattManagerClient() {
  return client_bundle_->bluetooth_gatt_manager_client();
}

BluetoothGattServiceClient*
DBusThreadManager::GetBluetoothGattServiceClient() {
  return client_bundle_->bluetooth_gatt_service_client();
}

BluetoothInputClient* DBusThreadManager::GetBluetoothInputClient() {
  return client_bundle_->bluetooth_input_client();
}

BluetoothProfileManagerClient*
DBusThreadManager::GetBluetoothProfileManagerClient() {
  return client_bundle_->bluetooth_profile_manager_client();
}

CrasAudioClient* DBusThreadManager::GetCrasAudioClient() {
  return client_bundle_->cras_audio_client();
}

CrosDisksClient* DBusThreadManager::GetCrosDisksClient() {
  return client_bundle_->cros_disks_client();
}

CryptohomeClient* DBusThreadManager::GetCryptohomeClient() {
  return client_bundle_->cryptohome_client();
}

DebugDaemonClient* DBusThreadManager::GetDebugDaemonClient() {
  return client_bundle_->debug_daemon_client();
}

EasyUnlockClient* DBusThreadManager::GetEasyUnlockClient() {
  return client_bundle_->easy_unlock_client();
}
LorgnetteManagerClient*
DBusThreadManager::GetLorgnetteManagerClient() {
  return client_bundle_->lorgnette_manager_client();
}

ShillDeviceClient*
DBusThreadManager::GetShillDeviceClient() {
  return client_bundle_->shill_device_client();
}

ShillIPConfigClient*
DBusThreadManager::GetShillIPConfigClient() {
  return client_bundle_->shill_ipconfig_client();
}

ShillManagerClient*
DBusThreadManager::GetShillManagerClient() {
  return client_bundle_->shill_manager_client();
}

ShillServiceClient*
DBusThreadManager::GetShillServiceClient() {
  return client_bundle_->shill_service_client();
}

ShillProfileClient*
DBusThreadManager::GetShillProfileClient() {
  return client_bundle_->shill_profile_client();
}

GsmSMSClient* DBusThreadManager::GetGsmSMSClient() {
  return client_bundle_->gsm_sms_client();
}

ImageBurnerClient* DBusThreadManager::GetImageBurnerClient() {
  return client_bundle_->image_burner_client();
}

IntrospectableClient* DBusThreadManager::GetIntrospectableClient() {
  return client_bundle_->introspectable_client();
}

ModemMessagingClient* DBusThreadManager::GetModemMessagingClient() {
  return client_bundle_->modem_messaging_client();
}

NfcAdapterClient* DBusThreadManager::GetNfcAdapterClient() {
  return client_bundle_->nfc_adapter_client();
}

NfcDeviceClient* DBusThreadManager::GetNfcDeviceClient() {
  return client_bundle_->nfc_device_client();
}

NfcManagerClient* DBusThreadManager::GetNfcManagerClient() {
  return client_bundle_->nfc_manager_client();
}

NfcRecordClient* DBusThreadManager::GetNfcRecordClient() {
  return client_bundle_->nfc_record_client();
}

NfcTagClient* DBusThreadManager::GetNfcTagClient() {
  return client_bundle_->nfc_tag_client();
}

PermissionBrokerClient* DBusThreadManager::GetPermissionBrokerClient() {
  return client_bundle_->permission_broker_client();
}

PowerManagerClient* DBusThreadManager::GetPowerManagerClient() {
  return client_bundle_->power_manager_client();
}

SessionManagerClient* DBusThreadManager::GetSessionManagerClient() {
  return client_bundle_->session_manager_client();
}

SMSClient* DBusThreadManager::GetSMSClient() {
  return client_bundle_->sms_client();
}

SystemClockClient* DBusThreadManager::GetSystemClockClient() {
  return client_bundle_->system_clock_client();
}

UpdateEngineClient* DBusThreadManager::GetUpdateEngineClient() {
  return client_bundle_->update_engine_client();
}

PowerPolicyController* DBusThreadManager::GetPowerPolicyController() {
  return power_policy_controller_.get();
}

void DBusThreadManager::InitializeClients() {
  GetBluetoothAdapterClient()->Init(GetSystemBus());
  GetBluetoothAgentManagerClient()->Init(GetSystemBus());
  GetBluetoothDeviceClient()->Init(GetSystemBus());
  GetBluetoothGattCharacteristicClient()->Init(GetSystemBus());
  GetBluetoothGattDescriptorClient()->Init(GetSystemBus());
  GetBluetoothGattManagerClient()->Init(GetSystemBus());
  GetBluetoothGattServiceClient()->Init(GetSystemBus());
  GetBluetoothInputClient()->Init(GetSystemBus());
  GetBluetoothProfileManagerClient()->Init(GetSystemBus());
  GetCrasAudioClient()->Init(GetSystemBus());
  GetCrosDisksClient()->Init(GetSystemBus());
  GetCryptohomeClient()->Init(GetSystemBus());
  GetDebugDaemonClient()->Init(GetSystemBus());
  GetEasyUnlockClient()->Init(GetSystemBus());
  GetGsmSMSClient()->Init(GetSystemBus());
  GetImageBurnerClient()->Init(GetSystemBus());
  GetIntrospectableClient()->Init(GetSystemBus());
  GetLorgnetteManagerClient()->Init(GetSystemBus());
  GetModemMessagingClient()->Init(GetSystemBus());
  GetPermissionBrokerClient()->Init(GetSystemBus());
  GetPowerManagerClient()->Init(GetSystemBus());
  GetSessionManagerClient()->Init(GetSystemBus());
  GetShillDeviceClient()->Init(GetSystemBus());
  GetShillIPConfigClient()->Init(GetSystemBus());
  GetShillManagerClient()->Init(GetSystemBus());
  GetShillServiceClient()->Init(GetSystemBus());
  GetShillProfileClient()->Init(GetSystemBus());
  GetSMSClient()->Init(GetSystemBus());
  GetSystemClockClient()->Init(GetSystemBus());
  GetUpdateEngineClient()->Init(GetSystemBus());

  // Initialize the NFC clients in the correct order. The order of
  // initialization matters due to dependencies that exist between the
  // client objects.
  GetNfcManagerClient()->Init(GetSystemBus());
  GetNfcAdapterClient()->Init(GetSystemBus());
  GetNfcDeviceClient()->Init(GetSystemBus());
  GetNfcTagClient()->Init(GetSystemBus());
  GetNfcRecordClient()->Init(GetSystemBus());

  // PowerPolicyController is dependent on PowerManagerClient, so
  // initialize it after the main list of clients.
  if (GetPowerPolicyController())
    GetPowerPolicyController()->Init(this);

  // This must be called after the list of clients so they've each had a
  // chance to register with their object g_dbus_thread_managers.
  if (GetSystemBus())
    GetSystemBus()->GetManagedObjects();

  client_bundle_->SetupDefaultEnvironment();
}

bool DBusThreadManager::IsUsingStub(DBusClientBundle::DBusClientType client) {
  return client_bundle_->IsUsingStub(client);
}

// static
void DBusThreadManager::Initialize() {
  // If we initialize DBusThreadManager twice we may also be shutting it down
  // early; do not allow that.
  if (g_using_dbus_thread_manager_for_testing)
    return;

  CHECK(!g_dbus_thread_manager);
  bool use_dbus_stub = !base::SysInfo::IsRunningOnChromeOS() ||
      CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kDbusStub);
  bool force_unstub_clients = CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kDbusUnstubClients);
  // Determine whether we use stub or real client implementations.
  if (force_unstub_clients) {
    InitializeWithPartialStub(
        CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            chromeos::switches::kDbusUnstubClients));
  } else if (use_dbus_stub) {
    InitializeWithStubs();
  } else {
    InitializeWithRealClients();
  }
}

// static
scoped_ptr<DBusThreadManagerSetter> DBusThreadManager::GetSetterForTesting() {
  if (!g_using_dbus_thread_manager_for_testing) {
    g_using_dbus_thread_manager_for_testing = true;
    InitializeWithStubs();
  }

  return make_scoped_ptr(new DBusThreadManagerSetter());
}

// static
void DBusThreadManager::CreateGlobalInstance(
    DBusClientBundle::DBusClientTypeMask unstub_client_mask) {
  CHECK(!g_dbus_thread_manager);
  g_dbus_thread_manager = new DBusThreadManager(
      make_scoped_ptr(new DBusClientBundle(unstub_client_mask)));
  g_dbus_thread_manager->InitializeClients();
}

// static
void DBusThreadManager::InitializeWithRealClients() {
  CreateGlobalInstance(~static_cast<DBusClientBundle::DBusClientTypeMask>(0));
  VLOG(1) << "DBusThreadManager initialized for Chrome OS";
}

// static
void DBusThreadManager::InitializeWithStubs() {
  CreateGlobalInstance(0 /* unstub_client_mask */);
  VLOG(1) << "DBusThreadManager created for testing";
}

// static
void DBusThreadManager::InitializeWithPartialStub(
    const std::string& unstub_clients) {
  DBusClientBundle::DBusClientTypeMask unstub_client_mask =
      DBusClientBundle::ParseUnstubList(unstub_clients);
  // We should have something parsed correctly here.
  LOG_IF(FATAL, unstub_client_mask == 0)
      << "Switch values for --" << chromeos::switches::kDbusUnstubClients
      << " cannot be parsed: " << unstub_clients;
  VLOG(1) << "DBusThreadManager initialized for mixed runtime environment";
  CreateGlobalInstance(unstub_client_mask);
}

// static
bool DBusThreadManager::IsInitialized() {
  return g_dbus_thread_manager != NULL;
}

// static
void DBusThreadManager::Shutdown() {
  // Ensure that we only shutdown DBusThreadManager once.
  CHECK(g_dbus_thread_manager);
  DBusThreadManager* dbus_thread_manager = g_dbus_thread_manager;
  g_dbus_thread_manager = NULL;
  g_using_dbus_thread_manager_for_testing = false;
  delete dbus_thread_manager;
  VLOG(1) << "DBusThreadManager Shutdown completed";
}

// static
DBusThreadManager* DBusThreadManager::Get() {
  CHECK(g_dbus_thread_manager)
      << "DBusThreadManager::Get() called before Initialize()";
  return g_dbus_thread_manager;
}

DBusThreadManagerSetter::DBusThreadManagerSetter() {
}

DBusThreadManagerSetter::~DBusThreadManagerSetter() {
}

void DBusThreadManagerSetter::SetBluetoothAdapterClient(
    scoped_ptr<BluetoothAdapterClient> client) {
  DBusThreadManager::Get()->client_bundle_->bluetooth_adapter_client_ =
      client.Pass();
}

void DBusThreadManagerSetter::SetBluetoothAgentManagerClient(
    scoped_ptr<BluetoothAgentManagerClient> client) {
  DBusThreadManager::Get()->client_bundle_->bluetooth_agent_manager_client_ =
      client.Pass();
}

void DBusThreadManagerSetter::SetBluetoothDeviceClient(
    scoped_ptr<BluetoothDeviceClient> client) {
  DBusThreadManager::Get()->client_bundle_->bluetooth_device_client_ =
      client.Pass();
}

void DBusThreadManagerSetter::SetBluetoothGattCharacteristicClient(
    scoped_ptr<BluetoothGattCharacteristicClient> client) {
  DBusThreadManager::Get()->client_bundle_->
      bluetooth_gatt_characteristic_client_ = client.Pass();
}

void DBusThreadManagerSetter::SetBluetoothGattDescriptorClient(
    scoped_ptr<BluetoothGattDescriptorClient> client) {
  DBusThreadManager::Get()->client_bundle_->bluetooth_gatt_descriptor_client_ =
      client.Pass();
}

void DBusThreadManagerSetter::SetBluetoothGattManagerClient(
    scoped_ptr<BluetoothGattManagerClient> client) {
  DBusThreadManager::Get()->client_bundle_->bluetooth_gatt_manager_client_ =
      client.Pass();
}

void DBusThreadManagerSetter::SetBluetoothGattServiceClient(
    scoped_ptr<BluetoothGattServiceClient> client) {
  DBusThreadManager::Get()->client_bundle_->bluetooth_gatt_service_client_ =
      client.Pass();
}

void DBusThreadManagerSetter::SetBluetoothInputClient(
    scoped_ptr<BluetoothInputClient> client) {
  DBusThreadManager::Get()->client_bundle_->bluetooth_input_client_ =
      client.Pass();
}

void DBusThreadManagerSetter::SetBluetoothProfileManagerClient(
    scoped_ptr<BluetoothProfileManagerClient> client) {
  DBusThreadManager::Get()->client_bundle_->bluetooth_profile_manager_client_ =
      client.Pass();
}

void DBusThreadManagerSetter::SetCrasAudioClient(
    scoped_ptr<CrasAudioClient> client) {
  DBusThreadManager::Get()->client_bundle_->cras_audio_client_ = client.Pass();
}

void DBusThreadManagerSetter::SetCrosDisksClient(
    scoped_ptr<CrosDisksClient> client) {
  DBusThreadManager::Get()->client_bundle_->cros_disks_client_ = client.Pass();
}

void DBusThreadManagerSetter::SetCryptohomeClient(
    scoped_ptr<CryptohomeClient> client) {
  DBusThreadManager::Get()->client_bundle_->cryptohome_client_ = client.Pass();
}

void DBusThreadManagerSetter::SetDebugDaemonClient(
    scoped_ptr<DebugDaemonClient> client) {
  DBusThreadManager::Get()->client_bundle_->debug_daemon_client_ =
      client.Pass();
}

void DBusThreadManagerSetter::SetEasyUnlockClient(
    scoped_ptr<EasyUnlockClient> client) {
  DBusThreadManager::Get()->client_bundle_->easy_unlock_client_ = client.Pass();
}

void DBusThreadManagerSetter::SetLorgnetteManagerClient(
    scoped_ptr<LorgnetteManagerClient> client) {
  DBusThreadManager::Get()->client_bundle_->lorgnette_manager_client_ =
      client.Pass();
}

void DBusThreadManagerSetter::SetShillDeviceClient(
    scoped_ptr<ShillDeviceClient> client) {
  DBusThreadManager::Get()->client_bundle_->shill_device_client_ =
      client.Pass();
}

void DBusThreadManagerSetter::SetShillIPConfigClient(
    scoped_ptr<ShillIPConfigClient> client) {
  DBusThreadManager::Get()->client_bundle_->shill_ipconfig_client_ =
      client.Pass();
}

void DBusThreadManagerSetter::SetShillManagerClient(
    scoped_ptr<ShillManagerClient> client) {
  DBusThreadManager::Get()->client_bundle_->shill_manager_client_ =
      client.Pass();
}

void DBusThreadManagerSetter::SetShillServiceClient(
    scoped_ptr<ShillServiceClient> client) {
  DBusThreadManager::Get()->client_bundle_->shill_service_client_ =
      client.Pass();
}

void DBusThreadManagerSetter::SetShillProfileClient(
    scoped_ptr<ShillProfileClient> client) {
  DBusThreadManager::Get()->client_bundle_->shill_profile_client_ =
      client.Pass();
}

void DBusThreadManagerSetter::SetGsmSMSClient(
    scoped_ptr<GsmSMSClient> client) {
  DBusThreadManager::Get()->client_bundle_->gsm_sms_client_ = client.Pass();
}

void DBusThreadManagerSetter::SetImageBurnerClient(
    scoped_ptr<ImageBurnerClient> client) {
  DBusThreadManager::Get()->client_bundle_->image_burner_client_ =
      client.Pass();
}

void DBusThreadManagerSetter::SetIntrospectableClient(
    scoped_ptr<IntrospectableClient> client) {
  DBusThreadManager::Get()->client_bundle_->introspectable_client_ =
      client.Pass();
}

void DBusThreadManagerSetter::SetModemMessagingClient(
    scoped_ptr<ModemMessagingClient> client) {
  DBusThreadManager::Get()->client_bundle_->modem_messaging_client_ =
      client.Pass();
}

void DBusThreadManagerSetter::SetNfcAdapterClient(
    scoped_ptr<NfcAdapterClient> client) {
  DBusThreadManager::Get()->client_bundle_->nfc_adapter_client_ = client.Pass();
}

void DBusThreadManagerSetter::SetNfcDeviceClient(
    scoped_ptr<NfcDeviceClient> client) {
  DBusThreadManager::Get()->client_bundle_->nfc_device_client_ = client.Pass();
}

void DBusThreadManagerSetter::SetNfcManagerClient(
    scoped_ptr<NfcManagerClient> client) {
  DBusThreadManager::Get()->client_bundle_->nfc_manager_client_ = client.Pass();
}

void DBusThreadManagerSetter::SetNfcRecordClient(
    scoped_ptr<NfcRecordClient> client) {
  DBusThreadManager::Get()->client_bundle_->nfc_record_client_ = client.Pass();
}

void DBusThreadManagerSetter::SetNfcTagClient(
    scoped_ptr<NfcTagClient> client) {
  DBusThreadManager::Get()->client_bundle_->nfc_tag_client_ = client.Pass();
}

void DBusThreadManagerSetter::SetPermissionBrokerClient(
    scoped_ptr<PermissionBrokerClient> client) {
  DBusThreadManager::Get()->client_bundle_->permission_broker_client_ =
      client.Pass();
}

void DBusThreadManagerSetter::SetPowerManagerClient(
    scoped_ptr<PowerManagerClient> client) {
  DBusThreadManager::Get()->power_policy_controller_.reset();
  DBusThreadManager::Get()->client_bundle_->power_manager_client_ =
      client.Pass();
  DBusThreadManager::Get()->power_policy_controller_.reset(
      new PowerPolicyController);
  DBusThreadManager::Get()->power_policy_controller_->Init(
      DBusThreadManager::Get());
}

void DBusThreadManagerSetter::SetSessionManagerClient(
    scoped_ptr<SessionManagerClient> client) {
  DBusThreadManager::Get()->client_bundle_->session_manager_client_ =
      client.Pass();
}

void DBusThreadManagerSetter::SetSMSClient(scoped_ptr<SMSClient> client) {
  DBusThreadManager::Get()->client_bundle_->sms_client_ = client.Pass();
}

void DBusThreadManagerSetter::SetSystemClockClient(
    scoped_ptr<SystemClockClient> client) {
  DBusThreadManager::Get()->client_bundle_->system_clock_client_ =
      client.Pass();
}

void DBusThreadManagerSetter::SetUpdateEngineClient(
    scoped_ptr<UpdateEngineClient> client) {
  DBusThreadManager::Get()->client_bundle_->update_engine_client_ =
      client.Pass();
}

}  // namespace chromeos
