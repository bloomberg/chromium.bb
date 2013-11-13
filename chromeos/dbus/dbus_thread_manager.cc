// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/dbus_thread_manager.h"

#include <map>

#include "base/command_line.h"
#include "base/observer_list.h"
#include "base/sys_info.h"
#include "base/threading/thread.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/bluetooth_adapter_client.h"
#include "chromeos/dbus/bluetooth_agent_manager_client.h"
#include "chromeos/dbus/bluetooth_device_client.h"
#include "chromeos/dbus/bluetooth_input_client.h"
#include "chromeos/dbus/bluetooth_profile_manager_client.h"
#include "chromeos/dbus/cras_audio_client.h"
#include "chromeos/dbus/cros_disks_client.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/dbus/dbus_client_implementation_type.h"
#include "chromeos/dbus/dbus_thread_manager_observer.h"
#include "chromeos/dbus/debug_daemon_client.h"
#include "chromeos/dbus/fake_dbus_thread_manager.h"
#include "chromeos/dbus/gsm_sms_client.h"
#include "chromeos/dbus/image_burner_client.h"
#include "chromeos/dbus/introspectable_client.h"
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
#include "chromeos/dbus/shill_stub_helper.h"
#include "chromeos/dbus/sms_client.h"
#include "chromeos/dbus/system_clock_client.h"
#include "chromeos/dbus/update_engine_client.h"
#include "dbus/bus.h"
#include "dbus/dbus_statistics.h"

namespace chromeos {

static DBusThreadManager* g_dbus_thread_manager = NULL;
static bool g_dbus_thread_manager_set_for_testing = false;

// The DBusThreadManager implementation used in production.
class DBusThreadManagerImpl : public DBusThreadManager {
 public:
  explicit DBusThreadManagerImpl() {
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
    FOR_EACH_OBSERVER(DBusThreadManagerObserver, observers_,
                      OnDBusThreadManagerDestroying(this));

    // Shut down the bus. During the browser shutdown, it's ok to shut down
    // the bus synchronously.
    system_bus_->ShutdownOnDBusThreadAndBlock();

    // Stop the D-Bus thread.
    dbus_thread_->Stop();
  }

  // DBusThreadManager overrides:
  virtual void AddObserver(DBusThreadManagerObserver* observer) OVERRIDE {
    DCHECK(observer);
    observers_.AddObserver(observer);
  }

  virtual void RemoveObserver(DBusThreadManagerObserver* observer) OVERRIDE {
    DCHECK(observer);
    observers_.RemoveObserver(observer);
  }

  virtual dbus::Bus* GetSystemBus() OVERRIDE {
    return system_bus_.get();
  }

  virtual BluetoothAdapterClient* GetBluetoothAdapterClient() OVERRIDE {
    return bluetooth_adapter_client_.get();
  }

  virtual BluetoothAgentManagerClient* GetBluetoothAgentManagerClient()
      OVERRIDE {
    return bluetooth_agent_manager_client_.get();
  }

  virtual BluetoothDeviceClient* GetBluetoothDeviceClient() OVERRIDE {
    return bluetooth_device_client_.get();
  }

  virtual BluetoothInputClient* GetBluetoothInputClient() OVERRIDE {
    return bluetooth_input_client_.get();
  }

  virtual BluetoothProfileManagerClient* GetBluetoothProfileManagerClient()
      OVERRIDE {
    return bluetooth_profile_manager_client_.get();
  }

  virtual CrasAudioClient* GetCrasAudioClient() OVERRIDE {
    return cras_audio_client_.get();
  }

  virtual CrosDisksClient* GetCrosDisksClient() OVERRIDE {
    return cros_disks_client_.get();
  }

  virtual CryptohomeClient* GetCryptohomeClient() OVERRIDE {
    return cryptohome_client_.get();
  }

  virtual DebugDaemonClient* GetDebugDaemonClient() OVERRIDE {
    return debug_daemon_client_.get();
  }

  virtual ShillDeviceClient* GetShillDeviceClient() OVERRIDE {
    return shill_device_client_.get();
  }

  virtual ShillIPConfigClient* GetShillIPConfigClient() OVERRIDE {
    return shill_ipconfig_client_.get();
  }

  virtual ShillManagerClient* GetShillManagerClient() OVERRIDE {
    return shill_manager_client_.get();
  }

  virtual ShillServiceClient* GetShillServiceClient() OVERRIDE {
    return shill_service_client_.get();
  }

  virtual ShillProfileClient* GetShillProfileClient() OVERRIDE {
    return shill_profile_client_.get();
  }

  virtual GsmSMSClient* GetGsmSMSClient() OVERRIDE {
    return gsm_sms_client_.get();
  }

  virtual ImageBurnerClient* GetImageBurnerClient() OVERRIDE {
    return image_burner_client_.get();
  }

  virtual IntrospectableClient* GetIntrospectableClient() OVERRIDE {
    return introspectable_client_.get();
  }

  virtual ModemMessagingClient* GetModemMessagingClient() OVERRIDE {
    return modem_messaging_client_.get();
  }

  virtual NfcAdapterClient* GetNfcAdapterClient() OVERRIDE {
    return nfc_adapter_client_.get();
  }

  virtual NfcDeviceClient* GetNfcDeviceClient() OVERRIDE {
    return nfc_device_client_.get();
  }

  virtual NfcManagerClient* GetNfcManagerClient() OVERRIDE {
    return nfc_manager_client_.get();
  }

  virtual NfcRecordClient* GetNfcRecordClient() OVERRIDE {
    return nfc_record_client_.get();
  }

  virtual NfcTagClient* GetNfcTagClient() OVERRIDE {
    return nfc_tag_client_.get();
  }

  virtual PermissionBrokerClient* GetPermissionBrokerClient() OVERRIDE {
    return permission_broker_client_.get();
  }

  virtual PowerManagerClient* GetPowerManagerClient() OVERRIDE {
    return power_manager_client_.get();
  }

  virtual PowerPolicyController* GetPowerPolicyController() OVERRIDE {
    return power_policy_controller_.get();
  }

  virtual SessionManagerClient* GetSessionManagerClient() OVERRIDE {
    return session_manager_client_.get();
  }

  virtual SMSClient* GetSMSClient() OVERRIDE {
    return sms_client_.get();
  }

  virtual SystemClockClient* GetSystemClockClient() OVERRIDE {
    return system_clock_client_.get();
  }

  virtual UpdateEngineClient* GetUpdateEngineClient() OVERRIDE {
    return update_engine_client_.get();
  }

 private:
  // Constructs all clients -- stub or real implementation according to
  // |client_type| and |client_type_override| -- and stores them in the
  // respective *_client_ member variable.
  void CreateDefaultClients() {
    DBusClientImplementationType client_type = REAL_DBUS_CLIENT_IMPLEMENTATION;
    DBusClientImplementationType client_type_override =
        REAL_DBUS_CLIENT_IMPLEMENTATION;
    // If --dbus-stub was requested, pass STUB to specific components;
    // Many components like login are not useful with a stub implementation.
    if (CommandLine::ForCurrentProcess()->HasSwitch(
            chromeos::switches::kDbusStub)) {
      client_type_override = STUB_DBUS_CLIENT_IMPLEMENTATION;
    }

    bluetooth_adapter_client_.reset(
        BluetoothAdapterClient::Create(client_type));
    bluetooth_agent_manager_client_.reset(
        BluetoothAgentManagerClient::Create(client_type));
    bluetooth_device_client_.reset(BluetoothDeviceClient::Create(client_type));
    bluetooth_input_client_.reset(BluetoothInputClient::Create(client_type));
    bluetooth_profile_manager_client_.reset(
        BluetoothProfileManagerClient::Create(client_type));
    cras_audio_client_.reset(CrasAudioClient::Create(client_type));
    cros_disks_client_.reset(CrosDisksClient::Create(client_type));
    cryptohome_client_.reset(CryptohomeClient::Create(client_type));
    debug_daemon_client_.reset(DebugDaemonClient::Create(client_type));
    shill_manager_client_.reset(
        ShillManagerClient::Create(client_type_override));
    shill_device_client_.reset(
        ShillDeviceClient::Create(client_type_override));
    shill_ipconfig_client_.reset(
        ShillIPConfigClient::Create(client_type_override));
    shill_service_client_.reset(
        ShillServiceClient::Create(client_type_override));
    shill_profile_client_.reset(
        ShillProfileClient::Create(client_type_override));
    gsm_sms_client_.reset(GsmSMSClient::Create(client_type_override));
    image_burner_client_.reset(ImageBurnerClient::Create(client_type));
    introspectable_client_.reset(IntrospectableClient::Create(client_type));
    modem_messaging_client_.reset(ModemMessagingClient::Create(client_type));
    // Create the NFC clients in the correct order based on their dependencies.
    nfc_manager_client_.reset(NfcManagerClient::Create(client_type));
    nfc_adapter_client_.reset(
        NfcAdapterClient::Create(client_type, nfc_manager_client_.get()));
    nfc_device_client_.reset(
        NfcDeviceClient::Create(client_type, nfc_adapter_client_.get()));
    nfc_tag_client_.reset(
        NfcTagClient::Create(client_type, nfc_adapter_client_.get()));
    nfc_record_client_.reset(
        NfcRecordClient::Create(
            client_type, nfc_device_client_.get(), nfc_tag_client_.get()));
    permission_broker_client_.reset(
        PermissionBrokerClient::Create(client_type));
    power_manager_client_.reset(
        PowerManagerClient::Create(client_type_override));
    session_manager_client_.reset(SessionManagerClient::Create(client_type));
    sms_client_.reset(SMSClient::Create(client_type));
    system_clock_client_.reset(SystemClockClient::Create(client_type));
    update_engine_client_.reset(UpdateEngineClient::Create(client_type));

    power_policy_controller_.reset(new PowerPolicyController);
  }

  // Note: Keep this before other members so they can call AddObserver() in
  // their c'tors.
  ObserverList<DBusThreadManagerObserver> observers_;

  scoped_ptr<base::Thread> dbus_thread_;
  scoped_refptr<dbus::Bus> system_bus_;
  scoped_ptr<BluetoothAdapterClient> bluetooth_adapter_client_;
  scoped_ptr<BluetoothAgentManagerClient> bluetooth_agent_manager_client_;
  scoped_ptr<BluetoothDeviceClient> bluetooth_device_client_;
  scoped_ptr<BluetoothInputClient> bluetooth_input_client_;
  scoped_ptr<BluetoothProfileManagerClient> bluetooth_profile_manager_client_;
  scoped_ptr<CrasAudioClient> cras_audio_client_;
  scoped_ptr<CrosDisksClient> cros_disks_client_;
  scoped_ptr<CryptohomeClient> cryptohome_client_;
  scoped_ptr<DebugDaemonClient> debug_daemon_client_;
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

  scoped_ptr<PowerPolicyController> power_policy_controller_;
};

// static
void DBusThreadManager::Initialize() {
  // Ignore Initialize() if we set a test DBusThreadManager.
  if (g_dbus_thread_manager_set_for_testing)
    return;
  // If we initialize DBusThreadManager twice we may also be shutting it down
  // early; do not allow that.
  CHECK(g_dbus_thread_manager == NULL);

  // Determine whether we use stub or real client implementations.
  if (base::SysInfo::IsRunningOnChromeOS()) {
    g_dbus_thread_manager = new DBusThreadManagerImpl;
    InitializeClients();
    VLOG(1) << "DBusThreadManager initialized for ChromeOS";
  } else {
    InitializeWithStub();
    return;
  }
}

// static
void DBusThreadManager::InitializeForTesting(
    DBusThreadManager* dbus_thread_manager) {
  // If we initialize DBusThreadManager twice we may also be shutting it down
  // early; do not allow that.
  CHECK(g_dbus_thread_manager == NULL);
  CHECK(dbus_thread_manager);
  g_dbus_thread_manager = dbus_thread_manager;
  g_dbus_thread_manager_set_for_testing = true;
  InitializeClients();
  VLOG(1) << "DBusThreadManager initialized with test implementation";
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
  shill_stub_helper::SetupDefaultEnvironment();
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
  CHECK(g_dbus_thread_manager || g_dbus_thread_manager_set_for_testing);
  DBusThreadManager* dbus_thread_manager = g_dbus_thread_manager;
  g_dbus_thread_manager = NULL;
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
  if (g_dbus_thread_manager_set_for_testing) {
    g_dbus_thread_manager = NULL;
    g_dbus_thread_manager_set_for_testing = false;
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
  InitClient(g_dbus_thread_manager->GetBluetoothInputClient());
  InitClient(g_dbus_thread_manager->GetBluetoothProfileManagerClient());
  InitClient(g_dbus_thread_manager->GetCrasAudioClient());
  InitClient(g_dbus_thread_manager->GetCrosDisksClient());
  InitClient(g_dbus_thread_manager->GetCryptohomeClient());
  InitClient(g_dbus_thread_manager->GetDebugDaemonClient());
  InitClient(g_dbus_thread_manager->GetGsmSMSClient());
  InitClient(g_dbus_thread_manager->GetImageBurnerClient());
  InitClient(g_dbus_thread_manager->GetIntrospectableClient());
  InitClient(g_dbus_thread_manager->GetModemMessagingClient());
  // Initialize the NFC clients in the correct order.
  InitClient(g_dbus_thread_manager->GetNfcAdapterClient());
  InitClient(g_dbus_thread_manager->GetNfcManagerClient());
  InitClient(g_dbus_thread_manager->GetNfcDeviceClient());
  InitClient(g_dbus_thread_manager->GetNfcTagClient());
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
