// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/testing_automation_provider.h"

#include "base/values.h"
#include "chrome/browser/automation/automation_provider_json.h"
#include "chrome/browser/automation/automation_provider_observers.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/cros/power_library.h"
#include "chrome/browser/chromeos/cros/screen_lock_library.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/screen_locker.h"

using chromeos::CrosLibrary;
using chromeos::NetworkLibrary;
using chromeos::UserManager;

namespace {

DictionaryValue* GetNetworkInfoDict(const chromeos::Network* network) {
  DictionaryValue* item = new DictionaryValue;
  item->SetString("name", network->name());
  item->SetString("device_path", network->device_path());
  item->SetString("ip_address", network->ip_address());
  item->SetString("status", network->GetStateString());
  return item;
}

}  // namespace

void TestingAutomationProvider::GetLoginInfo(DictionaryValue* args,
                                             IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);

  const UserManager* user_manager = UserManager::Get();
  if (!user_manager)
    reply.SendError("No user manager!");
  const chromeos::ScreenLocker* screen_locker =
      chromeos::ScreenLocker::default_screen_locker();

  return_value->SetBoolean("is_logged_in", user_manager->user_is_logged_in());
  return_value->SetBoolean("is_guest", user_manager->IsLoggedInAsGuest());
  return_value->SetBoolean("is_screen_locked", screen_locker);

  reply.SendSuccess(return_value.get());
}

// Logging in as guest will cause session_manager to restart Chrome with new
// flags. If you used EnableChromeTesting, you will have to call it again.
void TestingAutomationProvider::LoginAsGuest(DictionaryValue* args,
                                             IPC::Message* reply_message) {
  chromeos::ExistingUserController* controller =
      chromeos::ExistingUserController::current_controller();
  // Set up an observer (it will delete itself).
  new LoginManagerObserver(this, reply_message);
  controller->LoginAsGuest();
}

void TestingAutomationProvider::Login(DictionaryValue* args,
                                      IPC::Message* reply_message) {
  std::string username, password;
  if (!args->GetString("username", &username) ||
      !args->GetString("password", &password)) {
    AutomationJSONReply(this, reply_message).SendError(
        "Invalid or missing args.");
    return;
  }

  chromeos::ExistingUserController* controller =
      chromeos::ExistingUserController::current_controller();
  // Set up an observer (it will delete itself).
  new LoginManagerObserver(this, reply_message);
  controller->Login(username, password);
}

void TestingAutomationProvider::LockScreen(DictionaryValue* args,
                                           IPC::Message* reply_message) {
  if (!CrosLibrary::Get()->EnsureLoaded()) {
    AutomationJSONReply(this, reply_message).SendError(
        "Could not load cros library.");
    return;
  }

  new ScreenLockUnlockObserver(this, reply_message, true);
  CrosLibrary::Get()->GetScreenLockLibrary()->
      NotifyScreenLockRequested();
}

void TestingAutomationProvider::UnlockScreen(DictionaryValue* args,
                                             IPC::Message* reply_message) {
  if (!CrosLibrary::Get()->EnsureLoaded()) {
    AutomationJSONReply(this, reply_message).SendError(
        "Could not load cros library.");
    return;
  }

  new ScreenLockUnlockObserver(this, reply_message, false);
  CrosLibrary::Get()->GetScreenLockLibrary()->
      NotifyScreenUnlockRequested();
}

// Signing out could have undesirable side effects: session_manager is
// killed, so its children, including chrome and the window manager, will
// also be killed. Anything owned by chronos will probably be killed.
void TestingAutomationProvider::SignoutInScreenLocker(
    DictionaryValue* args, IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  chromeos::ScreenLocker* screen_locker =
      chromeos::ScreenLocker::default_screen_locker();
  if (!screen_locker) {
    reply.SendError(
        "No default screen locker. Are you sure the screen is locked?");
    return;
  }

  // Send success before stopping session because if we're a child of
  // session manager then we'll die when the session is stopped.
  reply.SendSuccess(NULL);
  screen_locker->Signout();
}

void TestingAutomationProvider::GetBatteryInfo(DictionaryValue* args,
                                               IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);

  if (!CrosLibrary::Get()->EnsureLoaded()) {
    reply.SendError("Could not load cros library.");
    return;
  }

  chromeos::PowerLibrary* power_library = CrosLibrary::Get()->GetPowerLibrary();
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);

  return_value->SetBoolean("battery_is_present",
                           power_library->battery_is_present());
  return_value->SetBoolean("line_power_on", power_library->line_power_on());
  if (power_library->battery_is_present()) {
    return_value->SetBoolean("battery_fully_charged",
                             power_library->battery_fully_charged());
    return_value->SetDouble("battery_percentage",
                            power_library->battery_percentage());
    if (power_library->line_power_on()) {
      int time = power_library->battery_time_to_full().InSeconds();
      if (time > 0 || power_library->battery_fully_charged())
        return_value->SetInteger("battery_time_to_full", time);
    } else {
      int time = power_library->battery_time_to_empty().InSeconds();
      if (time > 0)
        return_value->SetInteger("battery_time_to_empty", time);
    }
  }

  reply.SendSuccess(return_value.get());
}

void TestingAutomationProvider::GetNetworkInfo(DictionaryValue* args,
                                               IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);

  if (!CrosLibrary::Get()->EnsureLoaded()) {
    reply.SendError("Could not load cros library.");
    return;
  }

  NetworkLibrary* network_library = CrosLibrary::Get()->GetNetworkLibrary();
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);

  // IP address.
  return_value->SetString("ip_address", network_library->IPAddress());

  // Currently connected networks.
  if (network_library->ethernet_network())
    return_value->SetString(
        "connected_ethernet",
        network_library->ethernet_network()->service_path());
  if (network_library->wifi_network())
    return_value->SetString("connected_wifi",
                            network_library->wifi_network()->service_path());
  if (network_library->cellular_network())
    return_value->SetString(
        "connected_cellular",
        network_library->cellular_network()->service_path());

  // Ethernet network.
  bool ethernet_available = network_library->ethernet_available();
  bool ethernet_enabled = network_library->ethernet_enabled();
  if (ethernet_available && ethernet_enabled) {
    const chromeos::EthernetNetwork* ethernet_network =
        network_library->ethernet_network();
    if (ethernet_network) {
      DictionaryValue* items = new DictionaryValue;
      DictionaryValue* item = GetNetworkInfoDict(ethernet_network);
      items->Set(ethernet_network->service_path(), item);
      return_value->Set("ethernet_networks", items);
    }
  }

  // Wi-fi networks.
  bool wifi_available = network_library->wifi_available();
  bool wifi_enabled = network_library->wifi_enabled();
  if (wifi_available && wifi_enabled) {
    const chromeos::WifiNetworkVector& wifi_networks =
        network_library->wifi_networks();
    DictionaryValue* items = new DictionaryValue;
    for (size_t i = 0; i < wifi_networks.size(); ++i) {
      DictionaryValue* item = GetNetworkInfoDict(wifi_networks[i]);
      item->SetInteger("strength", wifi_networks[i]->strength());
      item->SetBoolean("encrypted", wifi_networks[i]->encrypted());
      item->SetString("encryption", wifi_networks[i]->GetEncryptionString());
      items->Set(wifi_networks[i]->service_path(), item);
    }
    return_value->Set("wifi_networks", items);
  }

  // Cellular networks.
  bool cellular_available = network_library->cellular_available();
  bool cellular_enabled = network_library->cellular_enabled();
  if (cellular_available && cellular_enabled) {
    const chromeos::CellularNetworkVector& cellular_networks =
        network_library->cellular_networks();
    DictionaryValue* items = new DictionaryValue;
    for (size_t i = 0; i < cellular_networks.size(); ++i) {
      DictionaryValue* item = GetNetworkInfoDict(cellular_networks[i]);
      item->SetInteger("strength", cellular_networks[i]->strength());
      item->SetString("operator_name", cellular_networks[i]->operator_name());
      item->SetString("operator_code", cellular_networks[i]->operator_code());
      item->SetString("payment_url", cellular_networks[i]->payment_url());
      item->SetString("usage_url", cellular_networks[i]->usage_url());
      item->SetString("network_technology",
                      cellular_networks[i]->GetNetworkTechnologyString());
      item->SetString("connectivity_state",
                      cellular_networks[i]->GetConnectivityStateString());
      item->SetString("activation_state",
                      cellular_networks[i]->GetActivationStateString());
      item->SetString("roaming_state",
                      cellular_networks[i]->GetRoamingStateString());
      items->Set(cellular_networks[i]->service_path(), item);
    }
    return_value->Set("cellular_networks", items);
  }

  reply.SendSuccess(return_value.get());
}

void TestingAutomationProvider::NetworkScan(DictionaryValue* args,
                                            IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);

  if (!CrosLibrary::Get()->EnsureLoaded()) {
    reply.SendError("Could not load cros library.");
    return;
  }

  NetworkLibrary* network_library = CrosLibrary::Get()->GetNetworkLibrary();
  network_library->RequestNetworkScan();

  // Set up an observer (it will delete itself).
  new NetworkScanObserver(this, reply_message);
}

void TestingAutomationProvider::ConnectToWifiNetwork(
    DictionaryValue* args, IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  std::string service_path, password, identity, certpath;
  if (!args->GetString("service_path", &service_path) ||
      !args->GetString("password", &password) ||
      !args->GetString("identity", &identity) ||
      !args->GetString("certpath", &certpath)) {
    reply.SendError("Invalid or missing args.");
    return;
  }

  if (!CrosLibrary::Get()->EnsureLoaded()) {
    reply.SendError("Could not load cros library.");
    return;
  }

  NetworkLibrary* network_library = CrosLibrary::Get()->GetNetworkLibrary();
  chromeos::WifiNetwork* wifi =
      network_library->FindWifiNetworkByPath(service_path);
  if (!wifi) {
    reply.SendError("No network found with specified service path.");
    return;
  }
  if (!password.empty())
    wifi->SetPassphrase(password);
  if (!identity.empty())
    wifi->SetIdentity(identity);
  if (!certpath.empty())
    wifi->SetCertPath(certpath);

  // Set up an observer (it will delete itself).
  new NetworkConnectObserver(this, reply_message, service_path);

  network_library->ConnectToWifiNetwork(wifi);
  network_library->RequestNetworkScan();
}

void TestingAutomationProvider::DisconnectFromWifiNetwork(
    DictionaryValue* args, IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  if (!CrosLibrary::Get()->EnsureLoaded()) {
    reply.SendError("Could not load cros library.");
    return;
  }

  NetworkLibrary* network_library = CrosLibrary::Get()->GetNetworkLibrary();
  const chromeos::WifiNetwork* wifi = network_library->wifi_network();
  if (!wifi) {
    reply.SendError("Not connected to any wifi network.");
    return;
  }

  network_library->DisconnectFromWirelessNetwork(wifi);
  reply.SendSuccess(NULL);
}
