// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/testing_automation_provider.h"

#include "base/values.h"
#include "chrome/browser/automation/automation_provider_json.h"
#include "chrome/browser/automation/automation_provider_observers.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/cros/screen_lock_library.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/screen_locker.h"

using chromeos::CrosLibrary;
using chromeos::NetworkLibrary;
using chromeos::UserManager;

namespace {

DictionaryValue* GetNetworkInfoDict(const chromeos::Network* network) {
  DictionaryValue* item = new DictionaryValue;
  item->SetString("service_path", network->service_path());
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
        "Invalid or missing args");
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

  // Ethernet network.
  bool ethernet_available = network_library->ethernet_available();
  bool ethernet_enabled = network_library->ethernet_enabled();
  if (ethernet_available && ethernet_enabled) {
    const chromeos::EthernetNetwork* ethernet_network =
        network_library->ethernet_network();
    if (ethernet_network)
      return_value->Set("ethernet_network",
                        GetNetworkInfoDict(ethernet_network));
  }

  // Wi-fi networks.
  bool wifi_available = network_library->wifi_available();
  bool wifi_enabled = network_library->wifi_enabled();
  if (wifi_available && wifi_enabled) {
    const chromeos::WifiNetworkVector& wifi_networks =
        network_library->wifi_networks();
    ListValue* items = new ListValue;
    for (size_t i = 0; i < wifi_networks.size(); ++i) {
      DictionaryValue* item = GetNetworkInfoDict(wifi_networks[i]);
      item->SetInteger("strength", wifi_networks[i]->strength());
      item->SetBoolean("encrypted", wifi_networks[i]->encrypted());
      item->SetString("encryption", wifi_networks[i]->GetEncryptionString());
      items->Append(item);
    }
    return_value->Set("wifi_networks", items);
  }

  // Cellular networks.
  bool cellular_available = network_library->cellular_available();
  bool cellular_enabled = network_library->cellular_enabled();
  if (cellular_available && cellular_enabled) {
    const chromeos::CellularNetworkVector& cellular_networks =
        network_library->cellular_networks();
    ListValue* items = new ListValue;
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
      items->Append(item);
    }
    return_value->Set("cellular_networks", items);
  }

  reply.SendSuccess(return_value.get());
}

void TestingAutomationProvider::ConnectToWifiNetwork(
    DictionaryValue* args, IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  std::string service_path, password, identity, certpath;
  if (!args->GetString("service_path", &service_path) ||
      !args->GetString("password", &password) ||
      !args->GetString("identity", &identity) ||
      !args->GetString("certpath", &certpath)) {
    reply.SendError("Invalid or missing args");
    return;
  }

  if (!CrosLibrary::Get()->EnsureLoaded()) {
    reply.SendError("Could not load cros library.");
    return;
  }

  NetworkLibrary* network_library = CrosLibrary::Get()->GetNetworkLibrary();
  if (!network_library->ConnectToWifiNetwork(
      service_path, password, identity, certpath)) {
    reply.SendError("Failed to connect");
    return;
  }

  reply.SendSuccess(NULL);
}
