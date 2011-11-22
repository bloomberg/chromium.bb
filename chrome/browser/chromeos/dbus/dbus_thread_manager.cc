// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/dbus_thread_manager.h"

#include "base/command_line.h"
#include "base/threading/thread.h"
#include "chrome/browser/chromeos/dbus/bluetooth_adapter_client.h"
#include "chrome/browser/chromeos/dbus/bluetooth_manager_client.h"
#include "chrome/browser/chromeos/dbus/cros_dbus_service.h"
#include "chrome/browser/chromeos/dbus/cros_disks_client.h"
#include "chrome/browser/chromeos/dbus/power_manager_client.h"
#include "chrome/browser/chromeos/dbus/sensors_client.h"
#include "chrome/browser/chromeos/dbus/session_manager_client.h"
#include "chrome/browser/chromeos/dbus/speech_synthesizer_client.h"
#include "chrome/browser/chromeos/dbus/update_engine_client.h"
#include "chrome/common/chrome_switches.h"
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

    // Create and start the cros D-Bus service.
    cros_dbus_service_.reset(CrosDBusService::Create(system_bus_.get()));
    cros_dbus_service_->Start();

    // Start monitoring sensors if needed.
    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
    if (command_line.HasSwitch(switches::kEnableSensors))
      sensors_client_.reset(SensorsClient::Create(system_bus_.get()));

    // Create the bluetooth clients.
    bluetooth_manager_client_.reset(BluetoothManagerClient::Create(
        system_bus_.get()));
    bluetooth_adapter_client_.reset(BluetoothAdapterClient::Create(
        system_bus_.get()));

    // Create the power manager client.
    power_manager_client_.reset(PowerManagerClient::Create(system_bus_.get()));
    // Create the session manager client.
    session_manager_client_.reset(
        SessionManagerClient::Create(system_bus_.get()));
    // Create the speech synthesizer client.
    speech_synthesizer_client_.reset(
        SpeechSynthesizerClient::Create(system_bus_.get()));
    // Create the cros-disks client.
    cros_disks_client_.reset(
        CrosDisksClient::Create(system_bus_.get()));
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
  virtual BluetoothAdapterClient* GetBluetoothAdapterClient() OVERRIDE {
    return bluetooth_adapter_client_.get();
  }

  // DBusThreadManager override.
  virtual BluetoothManagerClient* GetBluetoothManagerClient() OVERRIDE {
    return bluetooth_manager_client_.get();
  }

  // DBusThreadManager override.
  virtual PowerManagerClient* GetPowerManagerClient() OVERRIDE {
    return power_manager_client_.get();
  }

  // DBusThreadManager override.
  virtual SensorsClient* GetSensorsClient() OVERRIDE {
    return sensors_client_.get();
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
  virtual CrosDisksClient* GetCrosDisksClient() {
    return cros_disks_client_.get();
  }

  // DBusThreadManager override.
  virtual UpdateEngineClient* GetUpdateEngineClient() OVERRIDE {
    return update_engine_client_.get();
  }

  scoped_ptr<base::Thread> dbus_thread_;
  scoped_refptr<dbus::Bus> system_bus_;
  scoped_ptr<CrosDBusService> cros_dbus_service_;
  scoped_ptr<BluetoothAdapterClient> bluetooth_adapter_client_;
  scoped_ptr<BluetoothManagerClient> bluetooth_manager_client_;
  scoped_ptr<PowerManagerClient> power_manager_client_;
  scoped_ptr<SensorsClient> sensors_client_;
  scoped_ptr<SessionManagerClient> session_manager_client_;
  scoped_ptr<SpeechSynthesizerClient> speech_synthesizer_client_;
  scoped_ptr<CrosDisksClient> cros_disks_client_;
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
