// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/dbus_thread_manager.h"

#include "base/threading/thread.h"
#include "chrome/browser/chromeos/dbus/bluetooth_adapter_client.h"
#include "chrome/browser/chromeos/dbus/bluetooth_device_client.h"
#include "chrome/browser/chromeos/dbus/bluetooth_input_client.h"
#include "chrome/browser/chromeos/dbus/bluetooth_manager_client.h"
#include "chrome/browser/chromeos/dbus/bluetooth_node_client.h"
#include "chrome/browser/chromeos/dbus/cashew_client.h"
#include "chrome/browser/chromeos/dbus/cros_disks_client.h"
#include "chrome/browser/chromeos/dbus/cryptohome_client.h"
#include "chrome/browser/chromeos/dbus/flimflam_ipconfig_client.h"
#include "chrome/browser/chromeos/dbus/flimflam_network_client.h"
#include "chrome/browser/chromeos/dbus/image_burner_client.h"
#include "chrome/browser/chromeos/dbus/introspectable_client.h"
#include "chrome/browser/chromeos/dbus/power_manager_client.h"
#include "chrome/browser/chromeos/dbus/session_manager_client.h"
#include "chrome/browser/chromeos/dbus/speech_synthesizer_client.h"
#include "chrome/browser/chromeos/dbus/update_engine_client.h"
#include "dbus/bus.h"

namespace chromeos {

static DBusThreadManager* g_dbus_thread_manager = NULL;

// The DBusThreadManager implementation used in production.
class DBusThreadManagerImpl : public DBusThreadManager {
 public:
  DBusThreadManagerImpl() {
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
        system_bus_.get()));
    bluetooth_adapter_client_.reset(BluetoothAdapterClient::Create(
        system_bus_.get(), bluetooth_manager_client_.get()));
    bluetooth_device_client_.reset(BluetoothDeviceClient::Create(
        system_bus_.get(), bluetooth_adapter_client_.get()));
    bluetooth_input_client_.reset(BluetoothInputClient::Create(
        system_bus_.get(), bluetooth_adapter_client_.get()));
    bluetooth_node_client_.reset(BluetoothNodeClient::Create(
        system_bus_.get(), bluetooth_device_client_.get()));
    // Create the Cashew client.
    cashew_client_.reset(CashewClient::Create(system_bus_.get()));
    // Create the cros-disks client.
    cros_disks_client_.reset(
        CrosDisksClient::Create(system_bus_.get()));
    // Create the Cryptohome client.
    cryptohome_client_.reset(
        CryptohomeClient::Create(system_bus_.get()));
    // Create the Flimflam IPConfig client.
    flimflam_ipconfig_client_.reset(
        FlimflamIPConfigClient::Create(system_bus_.get()));
    // Create the Flimflam Network client.
    flimflam_network_client_.reset(
        FlimflamNetworkClient::Create(system_bus_.get()));
    // Create the image burner client.
    image_burner_client_.reset(ImageBurnerClient::Create(system_bus_.get()));
    // Create the introspectable object client.
    introspectable_client_.reset(
        IntrospectableClient::Create(system_bus_.get()));
    // Create the power manager client.
    power_manager_client_.reset(PowerManagerClient::Create(system_bus_.get()));
    // Create the session manager client.
    session_manager_client_.reset(
        SessionManagerClient::Create(system_bus_.get()));
    // Create the speech synthesizer client.
    speech_synthesizer_client_.reset(
        SpeechSynthesizerClient::Create(system_bus_.get()));
    // Create the update engine client.
    update_engine_client_.reset(
        UpdateEngineClient::Create(system_bus_.get()));
  }

  virtual ~DBusThreadManagerImpl() {
    // Shut down the bus. During the browser shutdown, it's ok to shut down
    // the bus synchronously.
    system_bus_->ShutdownOnDBusThreadAndBlock();

    // Stop the D-Bus thread.
    dbus_thread_->Stop();
  }

  // DBusThreadManager override.
  virtual dbus::Bus* GetSystemBus() OVERRIDE {
    return system_bus_.get();
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
  virtual FlimflamIPConfigClient* GetFlimflamIPConfigClient() OVERRIDE {
    return flimflam_ipconfig_client_.get();
  }

  virtual FlimflamNetworkClient* GetFlimflamNetworkClient() OVERRIDE {
    return flimflam_network_client_.get();
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
  scoped_ptr<BluetoothAdapterClient> bluetooth_adapter_client_;
  scoped_ptr<BluetoothDeviceClient> bluetooth_device_client_;
  scoped_ptr<BluetoothInputClient> bluetooth_input_client_;
  scoped_ptr<BluetoothManagerClient> bluetooth_manager_client_;
  scoped_ptr<BluetoothNodeClient> bluetooth_node_client_;
  scoped_ptr<CashewClient> cashew_client_;
  scoped_ptr<CrosDisksClient> cros_disks_client_;
  scoped_ptr<CryptohomeClient> cryptohome_client_;
  scoped_ptr<FlimflamIPConfigClient> flimflam_ipconfig_client_;
  scoped_ptr<FlimflamNetworkClient> flimflam_network_client_;
  scoped_ptr<ImageBurnerClient> image_burner_client_;
  scoped_ptr<IntrospectableClient> introspectable_client_;
  scoped_ptr<PowerManagerClient> power_manager_client_;
  scoped_ptr<SessionManagerClient> session_manager_client_;
  scoped_ptr<SpeechSynthesizerClient> speech_synthesizer_client_;
  scoped_ptr<UpdateEngineClient> update_engine_client_;
};

// static
void DBusThreadManager::Initialize() {
  if (g_dbus_thread_manager) {
    LOG(WARNING) << "DBusThreadManager was already initialized";
    return;
  }
  g_dbus_thread_manager = new DBusThreadManagerImpl;
  VLOG(1) << "DBusThreadManager initialized";
}

// static
void DBusThreadManager::InitializeForTesting(
    DBusThreadManager* dbus_thread_manager) {
  if (g_dbus_thread_manager) {
    LOG(WARNING) << "DBusThreadManager was already initialized";
    return;
  }
  g_dbus_thread_manager = dbus_thread_manager;
  VLOG(1) << "DBusThreadManager initialized";
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
