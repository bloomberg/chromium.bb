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
#include "chromeos/dbus/debug_daemon_client.h"
#include "chromeos/dbus/easy_unlock_client.h"
#include "chromeos/dbus/fake_dbus_thread_manager.h"
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
static DBusThreadManager* g_dbus_thread_manager_for_testing = NULL;

// The bundle of all D-Bus clients used in DBusThreadManagerImpl. The bundle
// is used to delete them at once in the right order before shutting down the
// system bus. See also the comment in the destructor of DBusThreadManagerImpl.
class DBusClientBundle {
 public:
  DBusClientBundle() {
    const DBusClientImplementationType type = REAL_DBUS_CLIENT_IMPLEMENTATION;

    bluetooth_adapter_client_.reset(BluetoothAdapterClient::Create());
    bluetooth_agent_manager_client_.reset(
        BluetoothAgentManagerClient::Create());
    bluetooth_device_client_.reset(BluetoothDeviceClient::Create());
    bluetooth_gatt_characteristic_client_.reset(
        BluetoothGattCharacteristicClient::Create());
    bluetooth_gatt_descriptor_client_.reset(
        BluetoothGattDescriptorClient::Create());
    bluetooth_gatt_manager_client_.reset(BluetoothGattManagerClient::Create());
    bluetooth_gatt_service_client_.reset(BluetoothGattServiceClient::Create());
    bluetooth_input_client_.reset(BluetoothInputClient::Create());
    bluetooth_profile_manager_client_.reset(
        BluetoothProfileManagerClient::Create());
    cras_audio_client_.reset(CrasAudioClient::Create());
    cros_disks_client_.reset(CrosDisksClient::Create(type));
    cryptohome_client_.reset(CryptohomeClient::Create());
    debug_daemon_client_.reset(DebugDaemonClient::Create());
    easy_unlock_client_.reset(EasyUnlockClient::Create());
    lorgnette_manager_client_.reset(LorgnetteManagerClient::Create());
    shill_manager_client_.reset(ShillManagerClient::Create());
    shill_device_client_.reset(ShillDeviceClient::Create());
    shill_ipconfig_client_.reset(ShillIPConfigClient::Create());
    shill_service_client_.reset(ShillServiceClient::Create());
    shill_profile_client_.reset(ShillProfileClient::Create());
    gsm_sms_client_.reset(GsmSMSClient::Create());
    image_burner_client_.reset(ImageBurnerClient::Create());
    introspectable_client_.reset(IntrospectableClient::Create());
    modem_messaging_client_.reset(ModemMessagingClient::Create());
    // Create the NFC clients in the correct order based on their dependencies.
    nfc_manager_client_.reset(NfcManagerClient::Create());
    nfc_adapter_client_.reset(
        NfcAdapterClient::Create(nfc_manager_client_.get()));
    nfc_device_client_.reset(
        NfcDeviceClient::Create(nfc_adapter_client_.get()));
    nfc_tag_client_.reset(NfcTagClient::Create(nfc_adapter_client_.get()));
    nfc_record_client_.reset(NfcRecordClient::Create(nfc_device_client_.get(),
                                                    nfc_tag_client_.get()));
    permission_broker_client_.reset(PermissionBrokerClient::Create());
    power_manager_client_.reset(PowerManagerClient::Create(type));
    session_manager_client_.reset(SessionManagerClient::Create(type));
    sms_client_.reset(SMSClient::Create());
    system_clock_client_.reset(SystemClockClient::Create());
    update_engine_client_.reset(UpdateEngineClient::Create(type));
  }

  BluetoothAdapterClient* bluetooth_adapter_client() {
    return bluetooth_adapter_client_.get();
  }
  BluetoothAgentManagerClient* bluetooth_agent_manager_client() {
    return bluetooth_agent_manager_client_.get();
  }
  BluetoothDeviceClient* bluetooth_device_client() {
    return bluetooth_device_client_.get();
  }
  BluetoothGattCharacteristicClient* bluetooth_gatt_characteristic_client() {
    return bluetooth_gatt_characteristic_client_.get();
  }
  BluetoothGattDescriptorClient* bluetooth_gatt_descriptor_client() {
    return bluetooth_gatt_descriptor_client_.get();
  }
  BluetoothGattManagerClient* bluetooth_gatt_manager_client() {
    return bluetooth_gatt_manager_client_.get();
  }
  BluetoothGattServiceClient* bluetooth_gatt_service_client() {
    return bluetooth_gatt_service_client_.get();
  }
  BluetoothInputClient* bluetooth_input_client() {
    return bluetooth_input_client_.get();
  }
  BluetoothProfileManagerClient* bluetooth_profile_manager_client() {
    return bluetooth_profile_manager_client_.get();
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
  GsmSMSClient* gsm_sms_client() {
    return gsm_sms_client_.get();
  }
  ImageBurnerClient* image_burner_client() {
    return image_burner_client_.get();
  }
  IntrospectableClient* introspectable_client() {
    return introspectable_client_.get();
  }
  ModemMessagingClient* modem_messaging_client() {
    return modem_messaging_client_.get();
  }
  NfcManagerClient* nfc_manager_client() {
    return nfc_manager_client_.get();
  }
  NfcAdapterClient* nfc_adapter_client() {
    return nfc_adapter_client_.get();
  }
  NfcDeviceClient* nfc_device_client() {
    return nfc_device_client_.get();
  }
  NfcTagClient* nfc_tag_client() {
    return nfc_tag_client_.get();
  }
  NfcRecordClient* nfc_record_client() {
    return nfc_record_client_.get();
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
  scoped_ptr<BluetoothAdapterClient> bluetooth_adapter_client_;
  scoped_ptr<BluetoothAgentManagerClient> bluetooth_agent_manager_client_;
  scoped_ptr<BluetoothDeviceClient> bluetooth_device_client_;
  scoped_ptr<BluetoothGattCharacteristicClient>
      bluetooth_gatt_characteristic_client_;
  scoped_ptr<BluetoothGattDescriptorClient> bluetooth_gatt_descriptor_client_;
  scoped_ptr<BluetoothGattManagerClient> bluetooth_gatt_manager_client_;
  scoped_ptr<BluetoothGattServiceClient> bluetooth_gatt_service_client_;
  scoped_ptr<BluetoothInputClient> bluetooth_input_client_;
  scoped_ptr<BluetoothProfileManagerClient> bluetooth_profile_manager_client_;
  scoped_ptr<CrasAudioClient> cras_audio_client_;
  scoped_ptr<CrosDisksClient> cros_disks_client_;
  scoped_ptr<CryptohomeClient> cryptohome_client_;
  scoped_ptr<DebugDaemonClient> debug_daemon_client_;
  scoped_ptr<EasyUnlockClient> easy_unlock_client_;
  scoped_ptr<LorgnetteManagerClient> lorgnette_manager_client_;
  scoped_ptr<ShillDeviceClient> shill_device_client_;
  scoped_ptr<ShillIPConfigClient> shill_ipconfig_client_;
  scoped_ptr<ShillManagerClient> shill_manager_client_;
  scoped_ptr<ShillServiceClient> shill_service_client_;
  scoped_ptr<ShillProfileClient> shill_profile_client_;
  scoped_ptr<GsmSMSClient> gsm_sms_client_;
  scoped_ptr<ImageBurnerClient> image_burner_client_;
  scoped_ptr<IntrospectableClient> introspectable_client_;
  scoped_ptr<ModemMessagingClient> modem_messaging_client_;
  // The declaration order for NFC client objects is important. See
  // DBusThreadManager::CreateDefaultClients for the dependencies.
  scoped_ptr<NfcManagerClient> nfc_manager_client_;
  scoped_ptr<NfcAdapterClient> nfc_adapter_client_;
  scoped_ptr<NfcDeviceClient> nfc_device_client_;
  scoped_ptr<NfcTagClient> nfc_tag_client_;
  scoped_ptr<NfcRecordClient> nfc_record_client_;
  scoped_ptr<PermissionBrokerClient> permission_broker_client_;
  scoped_ptr<SystemClockClient> system_clock_client_;
  scoped_ptr<PowerManagerClient> power_manager_client_;
  scoped_ptr<SessionManagerClient> session_manager_client_;
  scoped_ptr<SMSClient> sms_client_;
  scoped_ptr<UpdateEngineClient> update_engine_client_;

  DISALLOW_COPY_AND_ASSIGN(DBusClientBundle);
};

// The DBusThreadManager implementation used in production.
class DBusThreadManagerImpl : public DBusThreadManager {
 public:
  DBusThreadManagerImpl() {
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

    CreateDefaultClients();
  }

  virtual ~DBusThreadManagerImpl() {
    // PowerPolicyController's destructor depends on PowerManagerClient.
    power_policy_controller_.reset();

    // Delete all D-Bus clients before shutting down the system bus.
    client_bundle_.reset();

    // Shut down the bus. During the browser shutdown, it's ok to shut down
    // the bus synchronously.
    system_bus_->ShutdownOnDBusThreadAndBlock();

    // Stop the D-Bus thread.
    dbus_thread_->Stop();
  }

  // DBusThreadManager overrides:
  virtual dbus::Bus* GetSystemBus() OVERRIDE {
    return system_bus_.get();
  }

  virtual BluetoothAdapterClient* GetBluetoothAdapterClient() OVERRIDE {
    return client_bundle_->bluetooth_adapter_client();
  }

  virtual BluetoothAgentManagerClient* GetBluetoothAgentManagerClient()
      OVERRIDE {
    return client_bundle_->bluetooth_agent_manager_client();
  }

  virtual BluetoothDeviceClient* GetBluetoothDeviceClient() OVERRIDE {
    return client_bundle_->bluetooth_device_client();
  }

  virtual BluetoothGattCharacteristicClient*
      GetBluetoothGattCharacteristicClient() OVERRIDE {
    return client_bundle_->bluetooth_gatt_characteristic_client();
  }

  virtual BluetoothGattDescriptorClient* GetBluetoothGattDescriptorClient()
      OVERRIDE {
    return client_bundle_->bluetooth_gatt_descriptor_client();
  }

  virtual BluetoothGattManagerClient* GetBluetoothGattManagerClient() OVERRIDE {
    return client_bundle_->bluetooth_gatt_manager_client();
  }

  virtual BluetoothGattServiceClient* GetBluetoothGattServiceClient() OVERRIDE {
    return client_bundle_->bluetooth_gatt_service_client();
  }

  virtual BluetoothInputClient* GetBluetoothInputClient() OVERRIDE {
    return client_bundle_->bluetooth_input_client();
  }

  virtual BluetoothProfileManagerClient* GetBluetoothProfileManagerClient()
      OVERRIDE {
    return client_bundle_->bluetooth_profile_manager_client();
  }

  virtual CrasAudioClient* GetCrasAudioClient() OVERRIDE {
    return client_bundle_->cras_audio_client();
  }

  virtual CrosDisksClient* GetCrosDisksClient() OVERRIDE {
    return client_bundle_->cros_disks_client();
  }

  virtual CryptohomeClient* GetCryptohomeClient() OVERRIDE {
    return client_bundle_->cryptohome_client();
  }

  virtual DebugDaemonClient* GetDebugDaemonClient() OVERRIDE {
    return client_bundle_->debug_daemon_client();
  }

  virtual EasyUnlockClient* GetEasyUnlockClient() OVERRIDE {
    return client_bundle_->easy_unlock_client();
  }
  virtual LorgnetteManagerClient* GetLorgnetteManagerClient() OVERRIDE {
    return client_bundle_->lorgnette_manager_client();
  }

  virtual ShillDeviceClient* GetShillDeviceClient() OVERRIDE {
    return client_bundle_->shill_device_client();
  }

  virtual ShillIPConfigClient* GetShillIPConfigClient() OVERRIDE {
    return client_bundle_->shill_ipconfig_client();
  }

  virtual ShillManagerClient* GetShillManagerClient() OVERRIDE {
    return client_bundle_->shill_manager_client();
  }

  virtual ShillServiceClient* GetShillServiceClient() OVERRIDE {
    return client_bundle_->shill_service_client();
  }

  virtual ShillProfileClient* GetShillProfileClient() OVERRIDE {
    return client_bundle_->shill_profile_client();
  }

  virtual GsmSMSClient* GetGsmSMSClient() OVERRIDE {
    return client_bundle_->gsm_sms_client();
  }

  virtual ImageBurnerClient* GetImageBurnerClient() OVERRIDE {
    return client_bundle_->image_burner_client();
  }

  virtual IntrospectableClient* GetIntrospectableClient() OVERRIDE {
    return client_bundle_->introspectable_client();
  }

  virtual ModemMessagingClient* GetModemMessagingClient() OVERRIDE {
    return client_bundle_->modem_messaging_client();
  }

  virtual NfcAdapterClient* GetNfcAdapterClient() OVERRIDE {
    return client_bundle_->nfc_adapter_client();
  }

  virtual NfcDeviceClient* GetNfcDeviceClient() OVERRIDE {
    return client_bundle_->nfc_device_client();
  }

  virtual NfcManagerClient* GetNfcManagerClient() OVERRIDE {
    return client_bundle_->nfc_manager_client();
  }

  virtual NfcRecordClient* GetNfcRecordClient() OVERRIDE {
    return client_bundle_->nfc_record_client();
  }

  virtual NfcTagClient* GetNfcTagClient() OVERRIDE {
    return client_bundle_->nfc_tag_client();
  }

  virtual PermissionBrokerClient* GetPermissionBrokerClient() OVERRIDE {
    return client_bundle_->permission_broker_client();
  }

  virtual PowerManagerClient* GetPowerManagerClient() OVERRIDE {
    return client_bundle_->power_manager_client();
  }

  virtual SessionManagerClient* GetSessionManagerClient() OVERRIDE {
    return client_bundle_->session_manager_client();
  }

  virtual SMSClient* GetSMSClient() OVERRIDE {
    return client_bundle_->sms_client();
  }

  virtual SystemClockClient* GetSystemClockClient() OVERRIDE {
    return client_bundle_->system_clock_client();
  }

  virtual UpdateEngineClient* GetUpdateEngineClient() OVERRIDE {
    return client_bundle_->update_engine_client();
  }

  virtual PowerPolicyController* GetPowerPolicyController() OVERRIDE {
    return power_policy_controller_.get();
  }

 private:
  // Constructs all clients and stores them in the respective *_client_ member
  // variable.
  void CreateDefaultClients() {
    client_bundle_.reset(new DBusClientBundle);
    power_policy_controller_.reset(new PowerPolicyController);
  }

  scoped_ptr<base::Thread> dbus_thread_;
  scoped_refptr<dbus::Bus> system_bus_;
  scoped_ptr<DBusClientBundle> client_bundle_;
  scoped_ptr<PowerPolicyController> power_policy_controller_;

  DISALLOW_COPY_AND_ASSIGN(DBusThreadManagerImpl);
};

// static
void DBusThreadManager::Initialize() {
  // If we initialize DBusThreadManager twice we may also be shutting it down
  // early; do not allow that.
  CHECK(g_dbus_thread_manager == NULL);

  if (g_dbus_thread_manager_for_testing) {
    g_dbus_thread_manager = g_dbus_thread_manager_for_testing;
    InitializeClients();
    VLOG(1) << "DBusThreadManager initialized with test implementation";
    return;
  }

  // Determine whether we use stub or real client implementations.
  if (!base::SysInfo::IsRunningOnChromeOS() ||
      CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kDbusStub)) {
    InitializeWithStub();
  } else {
    g_dbus_thread_manager = new DBusThreadManagerImpl;
    InitializeClients();
    VLOG(1) << "DBusThreadManager initialized for Chrome OS";
  }
}

// static
void DBusThreadManager::SetInstanceForTesting(
    DBusThreadManager* dbus_thread_manager) {
  CHECK(!g_dbus_thread_manager);
  CHECK(!g_dbus_thread_manager_for_testing);
  g_dbus_thread_manager_for_testing = dbus_thread_manager;
}

// static
void DBusThreadManager::InitializeForTesting(
    DBusThreadManager* dbus_thread_manager) {
  SetInstanceForTesting(dbus_thread_manager);
  Initialize();
}

// static
void DBusThreadManager::InitializeWithStub() {
  // If we initialize DBusThreadManager twice we may also be shutting it down
  // early; do not allow that.
  CHECK(g_dbus_thread_manager == NULL);
  FakeDBusThreadManager* fake_dbus_thread_manager = new FakeDBusThreadManager;
  fake_dbus_thread_manager->SetFakeClients();
  g_dbus_thread_manager = fake_dbus_thread_manager;
  InitializeClients();
  fake_dbus_thread_manager->SetupDefaultEnvironment();
  VLOG(1) << "DBusThreadManager initialized with stub implementation";
}

// static
bool DBusThreadManager::IsInitialized() {
  return g_dbus_thread_manager != NULL;
}

// static
void DBusThreadManager::Shutdown() {
  // If we called InitializeForTesting, this may get called more than once.
  // Ensure that we only shutdown DBusThreadManager once.
  CHECK(g_dbus_thread_manager || g_dbus_thread_manager_for_testing);
  DBusThreadManager* dbus_thread_manager = g_dbus_thread_manager;
  g_dbus_thread_manager = NULL;
  g_dbus_thread_manager_for_testing = NULL;
  delete dbus_thread_manager;
  VLOG(1) << "DBusThreadManager Shutdown completed";
}

DBusThreadManager::DBusThreadManager() {
  dbus::statistics::Initialize();
}

DBusThreadManager::~DBusThreadManager() {
  dbus::statistics::Shutdown();
  if (g_dbus_thread_manager == NULL)
    return;  // Called form Shutdown() or local test instance.
  // There should never be both a global instance and a local instance.
  CHECK(this == g_dbus_thread_manager);
  if (g_dbus_thread_manager_for_testing) {
    g_dbus_thread_manager = NULL;
    g_dbus_thread_manager_for_testing = NULL;
    VLOG(1) << "DBusThreadManager destroyed";
  } else {
    LOG(FATAL) << "~DBusThreadManager() called outside of Shutdown()";
  }
}

// static
DBusThreadManager* DBusThreadManager::Get() {
  CHECK(g_dbus_thread_manager)
      << "DBusThreadManager::Get() called before Initialize()";
  return g_dbus_thread_manager;
}

// static
void DBusThreadManager::InitializeClients() {
  InitClient(g_dbus_thread_manager->GetBluetoothAdapterClient());
  InitClient(g_dbus_thread_manager->GetBluetoothAgentManagerClient());
  InitClient(g_dbus_thread_manager->GetBluetoothDeviceClient());
  InitClient(g_dbus_thread_manager->GetBluetoothGattCharacteristicClient());
  InitClient(g_dbus_thread_manager->GetBluetoothGattDescriptorClient());
  InitClient(g_dbus_thread_manager->GetBluetoothGattManagerClient());
  InitClient(g_dbus_thread_manager->GetBluetoothGattServiceClient());
  InitClient(g_dbus_thread_manager->GetBluetoothInputClient());
  InitClient(g_dbus_thread_manager->GetBluetoothProfileManagerClient());
  InitClient(g_dbus_thread_manager->GetCrasAudioClient());
  InitClient(g_dbus_thread_manager->GetCrosDisksClient());
  InitClient(g_dbus_thread_manager->GetCryptohomeClient());
  InitClient(g_dbus_thread_manager->GetDebugDaemonClient());
  InitClient(g_dbus_thread_manager->GetEasyUnlockClient());
  InitClient(g_dbus_thread_manager->GetGsmSMSClient());
  InitClient(g_dbus_thread_manager->GetImageBurnerClient());
  InitClient(g_dbus_thread_manager->GetIntrospectableClient());
  InitClient(g_dbus_thread_manager->GetLorgnetteManagerClient());
  InitClient(g_dbus_thread_manager->GetModemMessagingClient());
  InitClient(g_dbus_thread_manager->GetPermissionBrokerClient());
  InitClient(g_dbus_thread_manager->GetPowerManagerClient());
  InitClient(g_dbus_thread_manager->GetSessionManagerClient());
  InitClient(g_dbus_thread_manager->GetShillDeviceClient());
  InitClient(g_dbus_thread_manager->GetShillIPConfigClient());
  InitClient(g_dbus_thread_manager->GetShillManagerClient());
  InitClient(g_dbus_thread_manager->GetShillServiceClient());
  InitClient(g_dbus_thread_manager->GetShillProfileClient());
  InitClient(g_dbus_thread_manager->GetSMSClient());
  InitClient(g_dbus_thread_manager->GetSystemClockClient());
  InitClient(g_dbus_thread_manager->GetUpdateEngineClient());

  // Initialize the NFC clients in the correct order. The order of
  // initialization matters due to dependencies that exist between the
  // client objects.
  InitClient(g_dbus_thread_manager->GetNfcManagerClient());
  InitClient(g_dbus_thread_manager->GetNfcAdapterClient());
  InitClient(g_dbus_thread_manager->GetNfcDeviceClient());
  InitClient(g_dbus_thread_manager->GetNfcTagClient());
  InitClient(g_dbus_thread_manager->GetNfcRecordClient());

  // PowerPolicyController is dependent on PowerManagerClient, so
  // initialize it after the main list of clients.
  if (g_dbus_thread_manager->GetPowerPolicyController()) {
    g_dbus_thread_manager->GetPowerPolicyController()->Init(
        g_dbus_thread_manager);
  }

  // This must be called after the list of clients so they've each had a
  // chance to register with their object g_dbus_thread_managers.
  if (g_dbus_thread_manager->GetSystemBus())
    g_dbus_thread_manager->GetSystemBus()->GetManagedObjects();
}

// static
void DBusThreadManager::InitClient(DBusClient* client) {
  if (client)
    client->Init(g_dbus_thread_manager->GetSystemBus());
}

}  // namespace chromeos
