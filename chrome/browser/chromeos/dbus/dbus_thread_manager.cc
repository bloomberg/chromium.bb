// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/dbus_thread_manager.h"

#include "base/command_line.h"
#include "base/threading/thread.h"
#include "chrome/browser/chromeos/dbus/cros_dbus_service.h"
#include "chrome/browser/chromeos/dbus/sensors_source.h"
#include "chrome/common/chrome_switches.h"
#include "dbus/bus.h"

namespace chromeos {

static DBusThreadManager* g_dbus_thread_manager = NULL;

DBusThreadManager::DBusThreadManager() {
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
  cros_dbus_service_ = CrosDBusService::Get(system_bus_.get());
  cros_dbus_service_->Start();

  // Start monitoring sensors if needed.
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kEnableSensors)) {
    sensors_source_ = new SensorsSource;
    sensors_source_->Init(system_bus_.get());
  }
}

DBusThreadManager::~DBusThreadManager() {
  // Shut down the bus. During the browser shutdown, it's ok to shut down
  // the bus synchronously.
  system_bus_->ShutdownOnDBusThreadAndBlock();

  // Stop the D-Bus thread.
  dbus_thread_->Stop();

  // D-Bus clients should be deleted after the D-Bus thread is stopped.
  // See "CALLBACKS IN D-BUS CLIENTS" in the header file for why.
  delete cros_dbus_service_;
}

void DBusThreadManager::Initialize() {
  CHECK(!g_dbus_thread_manager);
  g_dbus_thread_manager = new DBusThreadManager;
  VLOG(1) << "DBusThreadManager initialized";
}

void DBusThreadManager::Shutdown() {
  if (!g_dbus_thread_manager) {
    // This can happen in tests.
    LOG(WARNING) << "DBusThreadManager::Shutdown() called with NULL manager";
    return;
  }
  delete g_dbus_thread_manager;
  g_dbus_thread_manager = NULL;
  VLOG(1) << "DBusThreadManager Shutdown completed";
}

DBusThreadManager* DBusThreadManager::Get() {
  CHECK(g_dbus_thread_manager)
      << "DBusThreadManager::Get() called before Initialize()";
  return g_dbus_thread_manager;
}

}  // namespace chromeos
