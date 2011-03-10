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

using chromeos::CrosLibrary;
using chromeos::NetworkLibrary;

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

// Logging out could have other undesirable side effects: session_manager is
// killed, so its children, including chrome and the window manager will also
// be killed. SSH connections might be closed, the test binary might be killed.
void TestingAutomationProvider::Logout(DictionaryValue* args,
                                       IPC::Message* reply_message) {
  // Send success before stopping session because if we're a child of
  // session manager then we'll die when the session is stopped.
  AutomationJSONReply(this, reply_message).SendSuccess(NULL);
  if (chromeos::CrosLibrary::Get()->EnsureLoaded())
    chromeos::CrosLibrary::Get()->GetLoginLibrary()->StopSession("");
}

void TestingAutomationProvider::ScreenLock(DictionaryValue* args,
                                           IPC::Message* reply_message) {
  new ScreenLockUnlockObserver(this, reply_message, true);
  chromeos::CrosLibrary::Get()->GetScreenLockLibrary()->
      NotifyScreenLockRequested();
}

void TestingAutomationProvider::ScreenUnlock(DictionaryValue* args,
                                             IPC::Message* reply_message) {
  new ScreenLockUnlockObserver(this, reply_message, false);
  chromeos::CrosLibrary::Get()->GetScreenLockLibrary()->
      NotifyScreenUnlockRequested();
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

