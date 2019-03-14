// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/dbus_helper.h"

#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/dbus/biod/biod_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/hammerd/hammerd_client.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/dbus/system_clock/system_clock_client.h"
#include "chromeos/dbus/upstart/upstart_client.h"
#include "chromeos/tpm/install_attributes.h"
#include "ui/base/ui_base_features.h"

namespace chromeos {

void InitializeDBus() {
  SystemSaltGetter::Initialize();

  // Initialize DBusThreadManager for the browser.
  DBusThreadManager::Initialize(DBusThreadManager::kAll);

  // Initialize Chrome dbus clients.
  dbus::Bus* bus = DBusThreadManager::Get()->GetSystemBus();

  // Features only needed in Ash. Initialize them here for non MultiProcessMash
  // to limit the number of places where dbus handlers are initialized. For
  // MultiProcessMash they are initialized in AshService::InitializeDBusClients.
  if (!::features::IsMultiProcessMash())
    chromeos::HammerdClient::Initialize(bus);

  PowerManagerClient::Initialize(bus);
  SystemClockClient::Initialize(bus);

  // TODO(stevenjb): Modify PowerManagerClient and SystemClockClient to use
  // the same pattern as UpstartClient.
  if (bus) {
    BiodClient::Initialize(bus);  // For device::Fingerprint.
    UpstartClient::Initialize(bus);
  } else {
    BiodClient::InitializeFake();  // For device::Fingerprint.
    UpstartClient::InitializeFake();
  }

  // Initialize the device settings service so that we'll take actions per
  // signals sent from the session manager. This needs to happen before
  // g_browser_process initializes BrowserPolicyConnector.
  DeviceSettingsService::Initialize();
  InstallAttributes::Initialize();
}

void ShutdownDBus() {
  UpstartClient::Shutdown();
  SystemClockClient::Shutdown();
  PowerManagerClient::Shutdown();
  BiodClient::Shutdown();

  // See comment in InitializeDBus() for MultiProcessMash behavior.
  if (!::features::IsMultiProcessMash())
    chromeos::HammerdClient::Shutdown();

  DBusThreadManager::Shutdown();
  SystemSaltGetter::Shutdown();
}

}  // namespace chromeos
