// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/dbus_thread_manager.h"

#include <map>

#include "base/chromeos/chromeos_version.h"
#include "base/command_line.h"
#include "base/observer_list.h"
#include "base/threading/thread.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/bluetooth_adapter_client.h"
#include "chromeos/dbus/bluetooth_device_client.h"
#include "chromeos/dbus/bluetooth_input_client.h"
#include "chromeos/dbus/bluetooth_manager_client.h"
#include "chromeos/dbus/bluetooth_node_client.h"
#include "chromeos/dbus/bluetooth_out_of_band_client.h"
#include "chromeos/dbus/cros_disks_client.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_client_implementation_type.h"
#include "chromeos/dbus/dbus_thread_manager_observer.h"
#include "chromeos/dbus/debug_daemon_client.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/dbus/shill_ipconfig_client.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/dbus/shill_profile_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/dbus/gsm_sms_client.h"
#include "chromeos/dbus/ibus/ibus_client.h"
#include "chromeos/dbus/ibus/ibus_config_client.h"
#include "chromeos/dbus/ibus/ibus_engine_factory_service.h"
#include "chromeos/dbus/ibus/ibus_engine_service.h"
#include "chromeos/dbus/ibus/ibus_input_context_client.h"
#include "chromeos/dbus/ibus/ibus_panel_service.h"
#include "chromeos/dbus/image_burner_client.h"
#include "chromeos/dbus/introspectable_client.h"
#include "chromeos/dbus/modem_messaging_client.h"
#include "chromeos/dbus/permission_broker_client.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/dbus/power_policy_controller.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/dbus/sms_client.h"
#include "chromeos/dbus/update_engine_client.h"
#include "dbus/bus.h"
#include "dbus/dbus_statistics.h"

namespace chromeos {

static DBusThreadManager* g_dbus_thread_manager = NULL;
static bool g_dbus_thread_manager_set_for_testing = false;

// The DBusThreadManager implementation used in production.
class DBusThreadManagerImpl : public DBusThreadManager {
 public:
  explicit DBusThreadManagerImpl(DBusClientImplementationType client_type) {
    // If --dbus-stub was requested, pass STUB to specific components;
    // Many components like login are not useful with a stub implementation.
    DBusClientImplementationType client_type_maybe_stub = client_type;
    if (CommandLine::ForCurrentProcess()->HasSwitch(
            chromeos::switches::kDbusStub))
      client_type_maybe_stub = STUB_DBUS_CLIENT_IMPLEMENTATION;

    // Create the D-Bus thread.
    base::Thread::Options thread_options;
    thread_options.message_loop_type = MessageLoop::TYPE_IO;
    dbus_thread_.reset(new base::Thread("D-Bus thread"));
    dbus_thread_->StartWithOptions(thread_options);

    // Create the connection to the system bus.
    dbus::Bus::Options system_bus_options;
    system_bus_options.bus_type = dbus::Bus::SYSTEM;
    system_bus_options.connection_type = dbus::Bus::PRIVATE;
    system_bus_options.dbus_task_runner = dbus_thread_->message_loop_proxy();
    system_bus_ = new dbus::Bus(system_bus_options);

    bluetooth_manager_client_.reset(BluetoothManagerClient::Create(
        client_type, system_bus_.get()));
    bluetooth_adapter_client_.reset(BluetoothAdapterClient::Create(
        client_type, system_bus_.get(), bluetooth_manager_client_.get()));
    bluetooth_device_client_.reset(BluetoothDeviceClient::Create(
        client_type, system_bus_.get(), bluetooth_adapter_client_.get()));
    bluetooth_input_client_.reset(BluetoothInputClient::Create(
        client_type, system_bus_.get(), bluetooth_adapter_client_.get()));
    bluetooth_node_client_.reset(BluetoothNodeClient::Create(
        client_type, system_bus_.get(), bluetooth_device_client_.get()));
    bluetooth_out_of_band_client_.reset(BluetoothOutOfBandClient::Create(
        client_type, system_bus_.get()));
    cros_disks_client_.reset(
        CrosDisksClient::Create(client_type, system_bus_.get()));
    cryptohome_client_.reset(
        CryptohomeClient::Create(client_type, system_bus_.get()));
    debug_daemon_client_.reset(
        DebugDaemonClient::Create(client_type, system_bus_.get()));
    shill_device_client_.reset(
        ShillDeviceClient::Create(client_type, system_bus_.get()));
    shill_ipconfig_client_.reset(
        ShillIPConfigClient::Create(client_type, system_bus_.get()));
    shill_manager_client_.reset(
        ShillManagerClient::Create(client_type, system_bus_.get()));
    shill_profile_client_.reset(
        ShillProfileClient::Create(client_type, system_bus_.get()));
    shill_service_client_.reset(
        ShillServiceClient::Create(client_type, system_bus_.get()));
    gsm_sms_client_.reset(
        GsmSMSClient::Create(client_type, system_bus_.get()));
    image_burner_client_.reset(ImageBurnerClient::Create(client_type,
                                                         system_bus_.get()));
    introspectable_client_.reset(
        IntrospectableClient::Create(client_type, system_bus_.get()));
    modem_messaging_client_.reset(
        ModemMessagingClient::Create(client_type, system_bus_.get()));
    permission_broker_client_.reset(
        PermissionBrokerClient::Create(client_type, system_bus_.get()));
    power_manager_client_.reset(
        PowerManagerClient::Create(client_type_maybe_stub, system_bus_.get()));
    session_manager_client_.reset(
        SessionManagerClient::Create(client_type, system_bus_.get()));
    sms_client_.reset(
        SMSClient::Create(client_type, system_bus_.get()));
    update_engine_client_.reset(
        UpdateEngineClient::Create(client_type, system_bus_.get()));

    // PowerPolicyController is dependent on PowerManagerClient, so
    // initialize it after the main list of clients.
    power_policy_controller_.reset(
        new PowerPolicyController(this, power_manager_client_.get()));
  }

  virtual ~DBusThreadManagerImpl() {
    FOR_EACH_OBSERVER(DBusThreadManagerObserver, observers_,
                      OnDBusThreadManagerDestroying(this));

    // Shut down the bus. During the browser shutdown, it's ok to shut down
    // the bus synchronously.
    system_bus_->ShutdownOnDBusThreadAndBlock();
    if (ibus_bus_.get())
      ibus_bus_->ShutdownOnDBusThreadAndBlock();

    // Release IBusEngineService instances.
    for (std::map<dbus::ObjectPath, IBusEngineService*>::iterator it
            = ibus_engine_services_.begin();
         it != ibus_engine_services_.end(); it++) {
      delete it->second;
    }

    // Stop the D-Bus thread.
    dbus_thread_->Stop();
  }

  // DBusThreadManager override.
  virtual void AddObserver(DBusThreadManagerObserver* observer) OVERRIDE {
    DCHECK(observer);
    observers_.AddObserver(observer);
  }

  // DBusThreadManager override.
  virtual void RemoveObserver(DBusThreadManagerObserver* observer) OVERRIDE {
    DCHECK(observer);
    observers_.RemoveObserver(observer);
  }

  // DBusThreadManager override.
  virtual void InitIBusBus(const std::string &ibus_address) OVERRIDE {
    DCHECK(!ibus_bus_);
    dbus::Bus::Options ibus_bus_options;
    ibus_bus_options.bus_type = dbus::Bus::CUSTOM_ADDRESS;
    ibus_bus_options.address = ibus_address;
    ibus_bus_options.connection_type = dbus::Bus::PRIVATE;
    ibus_bus_options.dbus_task_runner = dbus_thread_->message_loop_proxy();
    ibus_bus_ = new dbus::Bus(ibus_bus_options);
    ibus_address_ = ibus_address;
    VLOG(1) << "Connected to ibus-daemon: " << ibus_address;

    DBusClientImplementationType client_type =
        base::chromeos::IsRunningOnChromeOS() ? REAL_DBUS_CLIENT_IMPLEMENTATION
                                              : STUB_DBUS_CLIENT_IMPLEMENTATION;

    ibus_client_.reset(
        IBusClient::Create(client_type, ibus_bus_.get()));
    ibus_config_client_.reset(
        IBusConfigClient::Create(client_type, ibus_bus_.get()));
    ibus_input_context_client_.reset(
        IBusInputContextClient::Create(client_type));
    ibus_engine_factory_service_.reset(
        IBusEngineFactoryService::Create(ibus_bus_.get(), client_type));
    ibus_panel_service_.reset(
        IBusPanelService::Create(client_type,
                                 ibus_bus_.get(),
                                 ibus_input_context_client_.get()));

    ibus_engine_services_.clear();
  }

  // DBusThreadManager overrides:
  virtual dbus::Bus* GetSystemBus() OVERRIDE {
    return system_bus_.get();
  }

  virtual dbus::Bus* GetIBusBus() OVERRIDE {
    return ibus_bus_.get();
  }

  virtual BluetoothAdapterClient* GetBluetoothAdapterClient() OVERRIDE {
    return bluetooth_adapter_client_.get();
  }

  virtual BluetoothDeviceClient* GetBluetoothDeviceClient() OVERRIDE {
    return bluetooth_device_client_.get();
  }

  virtual BluetoothInputClient* GetBluetoothInputClient() OVERRIDE {
    return bluetooth_input_client_.get();
  }

  virtual BluetoothManagerClient* GetBluetoothManagerClient() OVERRIDE {
    return bluetooth_manager_client_.get();
  }

  virtual BluetoothNodeClient* GetBluetoothNodeClient() OVERRIDE {
    return bluetooth_node_client_.get();
  }

  virtual BluetoothOutOfBandClient* GetBluetoothOutOfBandClient() OVERRIDE {
    return bluetooth_out_of_band_client_.get();
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

  virtual ShillProfileClient* GetShillProfileClient() OVERRIDE {
    return shill_profile_client_.get();
  }

  virtual ShillServiceClient* GetShillServiceClient() OVERRIDE {
    return shill_service_client_.get();
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

  virtual UpdateEngineClient* GetUpdateEngineClient() OVERRIDE {
    return update_engine_client_.get();
  }

  virtual IBusClient* GetIBusClient() OVERRIDE {
    return ibus_client_.get();
  }

  virtual IBusConfigClient* GetIBusConfigClient() OVERRIDE {
    return ibus_config_client_.get();
  }

  virtual IBusInputContextClient* GetIBusInputContextClient() OVERRIDE {
    return ibus_input_context_client_.get();
  }

  virtual IBusEngineFactoryService* GetIBusEngineFactoryService() OVERRIDE {
    return ibus_engine_factory_service_.get();
  }

  virtual IBusEngineService* GetIBusEngineService(
      const dbus::ObjectPath& object_path) OVERRIDE {
    const DBusClientImplementationType client_type =
        base::chromeos::IsRunningOnChromeOS() ? REAL_DBUS_CLIENT_IMPLEMENTATION
                                              : STUB_DBUS_CLIENT_IMPLEMENTATION;

    if (ibus_engine_services_.find(object_path)
            == ibus_engine_services_.end()) {
      ibus_engine_services_[object_path] =
          IBusEngineService::Create(client_type, ibus_bus_.get(), object_path);
    }
    return ibus_engine_services_[object_path];
  }

  virtual void RemoveIBusEngineService(
      const dbus::ObjectPath& object_path) OVERRIDE {
    if (ibus_engine_services_.find(object_path) !=
        ibus_engine_services_.end()) {
      LOG(WARNING) << "Object path not found: " << object_path.value();
      return;
    }
    delete ibus_engine_services_[object_path];
    ibus_engine_services_.erase(object_path);
  }

  virtual IBusPanelService* GetIBusPanelService() OVERRIDE {
    return ibus_panel_service_.get();
  }

  // Note: Keep this before other members so they can call AddObserver() in
  // their c'tors.
  ObserverList<DBusThreadManagerObserver> observers_;

  scoped_ptr<base::Thread> dbus_thread_;
  scoped_refptr<dbus::Bus> system_bus_;
  scoped_refptr<dbus::Bus> ibus_bus_;
  scoped_ptr<BluetoothAdapterClient> bluetooth_adapter_client_;
  scoped_ptr<BluetoothDeviceClient> bluetooth_device_client_;
  scoped_ptr<BluetoothInputClient> bluetooth_input_client_;
  scoped_ptr<BluetoothManagerClient> bluetooth_manager_client_;
  scoped_ptr<BluetoothNodeClient> bluetooth_node_client_;
  scoped_ptr<BluetoothOutOfBandClient> bluetooth_out_of_band_client_;
  scoped_ptr<CrosDisksClient> cros_disks_client_;
  scoped_ptr<CryptohomeClient> cryptohome_client_;
  scoped_ptr<DebugDaemonClient> debug_daemon_client_;
  scoped_ptr<ShillDeviceClient> shill_device_client_;
  scoped_ptr<ShillIPConfigClient> shill_ipconfig_client_;
  scoped_ptr<ShillManagerClient> shill_manager_client_;
  scoped_ptr<ShillProfileClient> shill_profile_client_;
  scoped_ptr<ShillServiceClient> shill_service_client_;
  scoped_ptr<GsmSMSClient> gsm_sms_client_;
  scoped_ptr<ImageBurnerClient> image_burner_client_;
  scoped_ptr<IntrospectableClient> introspectable_client_;
  scoped_ptr<ModemMessagingClient> modem_messaging_client_;
  scoped_ptr<PermissionBrokerClient> permission_broker_client_;
  scoped_ptr<PowerManagerClient> power_manager_client_;
  scoped_ptr<SessionManagerClient> session_manager_client_;
  scoped_ptr<SMSClient> sms_client_;
  scoped_ptr<UpdateEngineClient> update_engine_client_;
  scoped_ptr<IBusClient> ibus_client_;
  scoped_ptr<IBusConfigClient> ibus_config_client_;
  scoped_ptr<IBusInputContextClient> ibus_input_context_client_;
  scoped_ptr<IBusEngineFactoryService> ibus_engine_factory_service_;
  std::map<dbus::ObjectPath, IBusEngineService*> ibus_engine_services_;
  scoped_ptr<IBusPanelService> ibus_panel_service_;
  scoped_ptr<PowerPolicyController> power_policy_controller_;

  std::string ibus_address_;
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
  if (base::chromeos::IsRunningOnChromeOS()) {
    g_dbus_thread_manager =
        new DBusThreadManagerImpl(REAL_DBUS_CLIENT_IMPLEMENTATION);
    VLOG(1) << "DBusThreadManager initialized for ChromeOS";
  } else {
    g_dbus_thread_manager =
        new DBusThreadManagerImpl(STUB_DBUS_CLIENT_IMPLEMENTATION);
    VLOG(1) << "DBusThreadManager initialized with Stub";
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
  VLOG(1) << "DBusThreadManager initialized with test implementation";
}

// static
void DBusThreadManager::InitializeWithStub() {
  // If we initialize DBusThreadManager twice we may also be shutting it down
  // early; do not allow that.
  CHECK(g_dbus_thread_manager == NULL);
  g_dbus_thread_manager =
        new DBusThreadManagerImpl(STUB_DBUS_CLIENT_IMPLEMENTATION);
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
  delete g_dbus_thread_manager;
  g_dbus_thread_manager = NULL;
  VLOG(1) << "DBusThreadManager Shutdown completed";
}

DBusThreadManager::DBusThreadManager() {
  dbus::statistics::Initialize();
}

DBusThreadManager::~DBusThreadManager() {
  dbus::statistics::Shutdown();
}

// static
DBusThreadManager* DBusThreadManager::Get() {
  CHECK(g_dbus_thread_manager)
      << "DBusThreadManager::Get() called before Initialize()";
  return g_dbus_thread_manager;
}

}  // namespace chromeos
