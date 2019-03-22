// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/dbus_helper.h"

#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/dbus/auth_policy/auth_policy_client.h"
#include "chromeos/dbus/biod/biod_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/hammerd/hammerd_client.h"
#include "chromeos/dbus/kerberos/kerberos_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/system_clock/system_clock_client.h"
#include "chromeos/dbus/upstart/upstart_client.h"
#include "chromeos/tpm/install_attributes.h"

namespace chromeos {

void InitializeDBus() {
  SystemSaltGetter::Initialize();

  // Initialize DBusThreadManager for the browser.
  DBusThreadManager::Initialize(DBusThreadManager::kAll);

  // Initialize Chrome dbus clients.
  dbus::Bus* bus = DBusThreadManager::Get()->GetSystemBus();

  // NOTE: base::Feature is not initialized yet, so any non MultiProcessMash
  // dbus client initialization for Ash should be done in Shell::Init.

  if (bus) {
    AuthPolicyClient::Initialize(bus);
    BiodClient::Initialize(bus);  // For device::Fingerprint.
    KerberosClient::Initialize(bus);
    PowerManagerClient::Initialize(bus);
    SystemClockClient::Initialize(bus);
    UpstartClient::Initialize(bus);
  } else {
    AuthPolicyClient::InitializeFake();
    BiodClient::InitializeFake();  // For device::Fingerprint.
    KerberosClient::InitializeFake();
    PowerManagerClient::InitializeFake();
    SystemClockClient::InitializeFake();
    UpstartClient::InitializeFake();
  }

  // Initialize the device settings service so that we'll take actions per
  // signals sent from the session manager. This needs to happen before
  // g_browser_process initializes BrowserPolicyConnector.
  DeviceSettingsService::Initialize();
  InstallAttributes::Initialize();
}

void ShutdownDBus() {
  AuthPolicyClient::Shutdown();
  UpstartClient::Shutdown();
  SystemClockClient::Shutdown();
  PowerManagerClient::Shutdown();
  KerberosClient::Shutdown();
  BiodClient::Shutdown();

  DBusThreadManager::Shutdown();
  SystemSaltGetter::Shutdown();
}

}  // namespace chromeos
