// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/dbus_thread_manager.h"

#include "base/chromeos/chromeos_version.h"
#include "base/command_line.h"
#include "base/threading/thread.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/bluetooth_adapter_client.h"
#include "chromeos/dbus/bluetooth_device_client.h"
#include "chromeos/dbus/bluetooth_input_client.h"
#include "chromeos/dbus/bluetooth_manager_client.h"
#include "chromeos/dbus/bluetooth_node_client.h"
#include "chromeos/dbus/cashew_client.h"
#include "chromeos/dbus/cros_disks_client.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_client_implementation_type.h"
#include "chromeos/dbus/debug_daemon_client.h"
#include "chromeos/dbus/flimflam_device_client.h"
#include "chromeos/dbus/flimflam_ipconfig_client.h"
#include "chromeos/dbus/flimflam_manager_client.h"
#include "chromeos/dbus/flimflam_network_client.h"
#include "chromeos/dbus/flimflam_profile_client.h"
#include "chromeos/dbus/flimflam_service_client.h"
#include "chromeos/dbus/gsm_sms_client.h"
#include "chromeos/dbus/image_burner_client.h"
#include "chromeos/dbus/introspectable_client.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/dbus/speech_synthesizer_client.h"
#include "chromeos/dbus/update_engine_client.h"
#include "dbus/bus.h"

namespace chromeos {

static DBusThreadManager* g_dbus_thread_manager = NULL;

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
    system_bus_options.dbus_thread_message_loop_proxy =
        dbus_thread_->message_loop_proxy();
    system_bus_ = new dbus::Bus(system_bus_options);

    // Create the bluetooth clients.
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
    // Create the Cashew client.
    cashew_client_.reset(CashewClient::Create(client_type, system_bus_.get()));
    // Create the cros-disks client.
    cros_disks_client_.reset(
        CrosDisksClient::Create(client_type, system_bus_.get()));
    // Create the Cryptohome client.
    cryptohome_client_.reset(
        CryptohomeClient::Create(client_type, system_bus_.get()));
    // Create the debugdaemon client.
    debugdaemon_client_.reset(
        DebugDaemonClient::Create(client_type, system_bus_.get()));
    // Create the Flimflam Device client.
    flimflam_device_client_.reset(
        FlimflamDeviceClient::Create(client_type, system_bus_.get()));
    // Create the Flimflam IPConfig client.
    flimflam_ipconfig_client_.reset(
        FlimflamIPConfigClient::Create(client_type, system_bus_.get()));
    // Create the Flimflam Manager client.
    flimflam_manager_client_.reset(
        FlimflamManagerClient::Create(client_type, system_bus_.get()));
    // Create the Flimflam Network client.
    flimflam_network_client_.reset(
        FlimflamNetworkClient::Create(client_type, system_bus_.get()));
    // Create the Flimflam Profile client.
    flimflam_profile_client_.reset(
        FlimflamProfileClient::Create(client_type, system_bus_.get()));
    // Create the Flimflam Service client.
    flimflam_service_client_.reset(
        FlimflamServiceClient::Create(client_type, system_bus_.get()));
    // Create the SMS cilent.
    gsm_sms_client_.reset(
        GsmSMSClient::Create(client_type, system_bus_.get()));
    // Create the image burner client.
    image_burner_client_.reset(ImageBurnerClient::Create(client_type,
                                                         system_bus_.get()));
    // Create the introspectable object client.
    introspectable_client_.reset(
        IntrospectableClient::Create(client_type, system_bus_.get()));
    // Create the power manager client.
    power_manager_client_.reset(
        PowerManagerClient::Create(client_type_maybe_stub, system_bus_.get()));
    // Create the session manager client.
    session_manager_client_.reset(
        SessionManagerClient::Create(client_type, system_bus_.get()));
    // Create the speech synthesizer client.
    speech_synthesizer_client_.reset(
        SpeechSynthesizerClient::Create(client_type, system_bus_.get()));
    // Create the update engine client.
    update_engine_client_.reset(
        UpdateEngineClient::Create(client_type, system_bus_.get()));
  }

  virtual ~DBusThreadManagerImpl() {
    // Shut down the bus. During the browser shutdown, it's ok to shut down
    // the bus synchronously.
    system_bus_->ShutdownOnDBusThreadAndBlock();
    if (ibus_bus_.get())
      ibus_bus_->ShutdownOnDBusThreadAndBlock();

    // Stop the D-Bus thread.
    dbus_thread_->Stop();
  }

  // DBusThreadManager override.
  virtual void InitIBusBus(const std::string &ibus_address) OVERRIDE {
    DCHECK(!ibus_bus_);
    dbus::Bus::Options ibus_bus_options;
    ibus_bus_options.bus_type = dbus::Bus::CUSTOM_ADDRESS;
    ibus_bus_options.address = ibus_address;
    ibus_bus_options.connection_type = dbus::Bus::PRIVATE;
    ibus_bus_options.dbus_thread_message_loop_proxy =
        dbus_thread_->message_loop_proxy();
    ibus_bus_ = new dbus::Bus(ibus_bus_options);
    ibus_address_ = ibus_address;
    VLOG(1) << "Connected to ibus-daemon: " << ibus_address;
  }

  // DBusThreadManager override.
  virtual dbus::Bus* GetSystemBus() OVERRIDE {
    return system_bus_.get();
  }

  // DBusThreadManager override.
  virtual dbus::Bus* GetIBusBus() OVERRIDE {
    return ibus_bus_.get();
  }

  // DBusThreadManager override.
  virtual BluetoothAdapterClient* GetBluetoothAdapterClient() OVERRIDE {
    return bluetooth_adapter_client_.get();
  }

  // DBusThreadManager override.
  virtual BluetoothDeviceClient* GetBluetoothDeviceClient() OVERRIDE {
    return bluetooth_device_client_.get();
  }

  // DBusThreadManager override.
  virtual BluetoothInputClient* GetBluetoothInputClient() OVERRIDE {
    return bluetooth_input_client_.get();
  }

  // DBusThreadManager override.
  virtual BluetoothManagerClient* GetBluetoothManagerClient() OVERRIDE {
    return bluetooth_manager_client_.get();
  }

  // DBusThreadManager override.
  virtual BluetoothNodeClient* GetBluetoothNodeClient() OVERRIDE {
    return bluetooth_node_client_.get();
  }

  // DBusThreadManager override.
  virtual CashewClient* GetCashewClient() OVERRIDE {
    return cashew_client_.get();
  }

  // DBusThreadManager override.
  virtual CrosDisksClient* GetCrosDisksClient() OVERRIDE {
    return cros_disks_client_.get();
  }

  // DBusThreadManager override.
  virtual CryptohomeClient* GetCryptohomeClient() OVERRIDE {
    return cryptohome_client_.get();
  }

  // DBusThreadManager override.
  virtual DebugDaemonClient* GetDebugDaemonClient() OVERRIDE {
    return debugdaemon_client_.get();
  }

  // DBusThreadManager override.
  virtual FlimflamDeviceClient* GetFlimflamDeviceClient() OVERRIDE {
    return flimflam_device_client_.get();
  }

  // DBusThreadManager override.
  virtual FlimflamIPConfigClient* GetFlimflamIPConfigClient() OVERRIDE {
    return flimflam_ipconfig_client_.get();
  }

  // DBusThreadManager override.
  virtual FlimflamManagerClient* GetFlimflamManagerClient() OVERRIDE {
    return flimflam_manager_client_.get();
  }

  // DBusThreadManager override.
  virtual FlimflamNetworkClient* GetFlimflamNetworkClient() OVERRIDE {
    return flimflam_network_client_.get();
  }

  // DBusThreadManager override.
  virtual FlimflamProfileClient* GetFlimflamProfileClient() OVERRIDE {
    return flimflam_profile_client_.get();
  }

  // DBusThreadManager override.
  virtual FlimflamServiceClient* GetFlimflamServiceClient() OVERRIDE {
    return flimflam_service_client_.get();
  }

  // DBusThreadManager override.
  virtual GsmSMSClient* GetGsmSMSClient() OVERRIDE {
    return gsm_sms_client_.get();
  }

  // DBusThreadManager override.
  virtual ImageBurnerClient* GetImageBurnerClient() OVERRIDE {
    return image_burner_client_.get();
  }

  // DBusThreadManager override.
  virtual IntrospectableClient* GetIntrospectableClient() OVERRIDE {
    return introspectable_client_.get();
  }

  // DBusThreadManager override.
  virtual PowerManagerClient* GetPowerManagerClient() OVERRIDE {
    return power_manager_client_.get();
  }

  // DBusThreadManager override.
  virtual SessionManagerClient* GetSessionManagerClient() OVERRIDE {
    return session_manager_client_.get();
  }

  // DBusThreadManager override.
  virtual SpeechSynthesizerClient* GetSpeechSynthesizerClient() OVERRIDE {
    return speech_synthesizer_client_.get();
  }

  // DBusThreadManager override.
  virtual UpdateEngineClient* GetUpdateEngineClient() OVERRIDE {
    return update_engine_client_.get();
  }

  scoped_ptr<base::Thread> dbus_thread_;
  scoped_refptr<dbus::Bus> system_bus_;
  scoped_refptr<dbus::Bus> ibus_bus_;
  scoped_ptr<BluetoothAdapterClient> bluetooth_adapter_client_;
  scoped_ptr<BluetoothDeviceClient> bluetooth_device_client_;
  scoped_ptr<BluetoothInputClient> bluetooth_input_client_;
  scoped_ptr<BluetoothManagerClient> bluetooth_manager_client_;
  scoped_ptr<BluetoothNodeClient> bluetooth_node_client_;
  scoped_ptr<CashewClient> cashew_client_;
  scoped_ptr<CrosDisksClient> cros_disks_client_;
  scoped_ptr<CryptohomeClient> cryptohome_client_;
  scoped_ptr<DebugDaemonClient> debugdaemon_client_;
  scoped_ptr<FlimflamDeviceClient> flimflam_device_client_;
  scoped_ptr<FlimflamIPConfigClient> flimflam_ipconfig_client_;
  scoped_ptr<FlimflamManagerClient> flimflam_manager_client_;
  scoped_ptr<FlimflamNetworkClient> flimflam_network_client_;
  scoped_ptr<FlimflamProfileClient> flimflam_profile_client_;
  scoped_ptr<FlimflamServiceClient> flimflam_service_client_;
  scoped_ptr<GsmSMSClient> gsm_sms_client_;
  scoped_ptr<ImageBurnerClient> image_burner_client_;
  scoped_ptr<IntrospectableClient> introspectable_client_;
  scoped_ptr<PowerManagerClient> power_manager_client_;
  scoped_ptr<SessionManagerClient> session_manager_client_;
  scoped_ptr<SpeechSynthesizerClient> speech_synthesizer_client_;
  scoped_ptr<UpdateEngineClient> update_engine_client_;

  std::string ibus_address_;
};

// static
void DBusThreadManager::Initialize() {
  if (g_dbus_thread_manager) {
    LOG(WARNING) << "DBusThreadManager was already initialized";
    return;
  }
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
  if (g_dbus_thread_manager) {
    LOG(WARNING) << "DBusThreadManager was already initialized";
    return;
  }
  CHECK(dbus_thread_manager);
  g_dbus_thread_manager = dbus_thread_manager;
  VLOG(1) << "DBusThreadManager initialized with test implementation";
}

// static
void DBusThreadManager::InitializeWithStub() {
  g_dbus_thread_manager =
        new DBusThreadManagerImpl(STUB_DBUS_CLIENT_IMPLEMENTATION);
    VLOG(1) << "DBusThreadManager initialized with stub implementation";
}

// static
void DBusThreadManager::Shutdown() {
  if (!g_dbus_thread_manager) {
    // TODO(satorux): Make it a DCHECK() once it's ready.
    LOG(WARNING) << "DBusThreadManager::Shutdown() called with NULL manager";
    return;
  }
  delete g_dbus_thread_manager;
  g_dbus_thread_manager = NULL;
  VLOG(1) << "DBusThreadManager Shutdown completed";
}

DBusThreadManager::DBusThreadManager() {
}

DBusThreadManager::~DBusThreadManager() {
}

// static
DBusThreadManager* DBusThreadManager::Get() {
  CHECK(g_dbus_thread_manager)
      << "DBusThreadManager::Get() called before Initialize()";
  return g_dbus_thread_manager;
}

}  // namespace chromeos
