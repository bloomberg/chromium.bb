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
#include "chrome/browser/chromeos/cros/update_library.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/chromeos/proxy_cros_settings_provider.h"

using chromeos::CrosLibrary;
using chromeos::NetworkLibrary;
using chromeos::UserManager;
using chromeos::UpdateLibrary;

namespace {

bool EnsureCrosLibraryLoaded(AutomationProvider* provider,
                             IPC::Message* reply_message) {
  if (!CrosLibrary::Get()->EnsureLoaded()) {
    AutomationJSONReply(provider, reply_message).SendError(
        "Could not load cros library.");
    return false;
  }
  return true;
}

DictionaryValue* GetNetworkInfoDict(const chromeos::Network* network) {
  DictionaryValue* item = new DictionaryValue;
  item->SetString("name", network->name());
  item->SetString("device_path", network->device_path());
  item->SetString("ip_address", network->ip_address());
  item->SetString("status", network->GetStateString());
  return item;
}

Value* GetProxySetting(const std::string& setting_name) {
  chromeos::ProxyCrosSettingsProvider settings_provider;
  std::string setting_path = "cros.session.proxy.";
  setting_path.append(setting_name);

  if (setting_name == "ignorelist") {
    Value* value;
    if (settings_provider.Get(setting_path, &value))
      return value;
  } else {
    Value* setting;
    if (settings_provider.Get(setting_path, &setting)) {
      DictionaryValue* setting_dict = static_cast<DictionaryValue*>(setting);
      Value* value;
      bool found = setting_dict->Remove("value", &value);
      delete setting;
      if (found)
        return value;
    }
  }
  return NULL;
}

const char* UpdateStatusToString(chromeos::UpdateStatusOperation status) {
  switch (status) {
    case chromeos::UPDATE_STATUS_IDLE:
      return "idle";
    case chromeos::UPDATE_STATUS_CHECKING_FOR_UPDATE:
      return "checking for update";
    case chromeos::UPDATE_STATUS_UPDATE_AVAILABLE:
      return "update available";
    case chromeos::UPDATE_STATUS_DOWNLOADING:
      return "downloading";
    case chromeos::UPDATE_STATUS_VERIFYING:
      return "verifying";
    case chromeos::UPDATE_STATUS_FINALIZING:
      return "finalizing";
    case chromeos::UPDATE_STATUS_UPDATED_NEED_REBOOT:
      return "updated need reboot";
    case chromeos::UPDATE_STATUS_REPORTING_ERROR_EVENT:
      return "reporting error event";
    default:
      return "unknown";
  }
}

void GetReleaseTrackCallback(void* user_data, const char* track) {
  AutomationJSONReply* reply = static_cast<AutomationJSONReply*>(user_data);

  if (track == NULL) {
    reply->SendError("Unable to get release track.");
    delete reply;
    return;
  }

  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  return_value->SetString("release_track", track);

  UpdateLibrary* update_library = CrosLibrary::Get()->GetUpdateLibrary();
  const UpdateLibrary::Status& status = update_library->status();
  chromeos::UpdateStatusOperation update_status = status.status;
  return_value->SetString("status", UpdateStatusToString(update_status));
  if (update_status == chromeos::UPDATE_STATUS_DOWNLOADING)
    return_value->SetDouble("download_progress", status.download_progress);
  if (status.last_checked_time > 0)
    return_value->SetInteger("last_checked_time", status.last_checked_time);
  if (status.new_size > 0)
    return_value->SetInteger("new_size", status.new_size);

  reply->SendSuccess(return_value.get());
  delete reply;
}

void UpdateCheckCallback(void* user_data, chromeos::UpdateResult result,
                         const char* error_msg) {
  AutomationJSONReply* reply = static_cast<AutomationJSONReply*>(user_data);
  if (result == chromeos::UPDATE_RESULT_SUCCESS)
    reply->SendSuccess(NULL);
  else
    reply->SendError(error_msg);
  delete reply;
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

  return_value->SetBoolean("is_owner", user_manager->current_user_is_owner());
  return_value->SetBoolean("is_logged_in", user_manager->user_is_logged_in());
  return_value->SetBoolean("is_screen_locked", screen_locker);
  if (user_manager->user_is_logged_in()) {
    return_value->SetBoolean("is_guest", user_manager->IsLoggedInAsGuest());
    return_value->SetString("email", user_manager->logged_in_user().email());
  }

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
  if (!EnsureCrosLibraryLoaded(this, reply_message))
    return;

  new ScreenLockUnlockObserver(this, reply_message, true);
  CrosLibrary::Get()->GetScreenLockLibrary()->
      NotifyScreenLockRequested();
}

void TestingAutomationProvider::UnlockScreen(DictionaryValue* args,
                                             IPC::Message* reply_message) {
  if (!EnsureCrosLibraryLoaded(this, reply_message))
    return;

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
  if (!EnsureCrosLibraryLoaded(this, reply_message))
    return;

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

  AutomationJSONReply(this, reply_message).SendSuccess(return_value.get());
}

void TestingAutomationProvider::GetNetworkInfo(DictionaryValue* args,
                                               IPC::Message* reply_message) {
  if (!EnsureCrosLibraryLoaded(this, reply_message))
    return;

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
    for (chromeos::WifiNetworkVector::const_iterator iter =
         wifi_networks.begin(); iter != wifi_networks.end(); ++iter) {
      const chromeos::WifiNetwork* wifi = *iter;
      DictionaryValue* item = GetNetworkInfoDict(wifi);
      item->SetInteger("strength", wifi->strength());
      item->SetBoolean("encrypted", wifi->encrypted());
      item->SetString("encryption", wifi->GetEncryptionString());
      items->Set(wifi->service_path(), item);
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

  AutomationJSONReply(this, reply_message).SendSuccess(return_value.get());
}

void TestingAutomationProvider::NetworkScan(DictionaryValue* args,
                                            IPC::Message* reply_message) {
  if (!EnsureCrosLibraryLoaded(this, reply_message))
    return;

  NetworkLibrary* network_library = CrosLibrary::Get()->GetNetworkLibrary();
  network_library->RequestNetworkScan();

  // Set up an observer (it will delete itself).
  new NetworkScanObserver(this, reply_message);
}

void TestingAutomationProvider::GetProxySettings(DictionaryValue* args,
                                                 IPC::Message* reply_message) {
  const char* settings[] = { "pacurl", "singlehttp", "singlehttpport",
                             "httpurl", "httpport", "httpsurl", "httpsport",
                             "type", "single", "ftpurl", "ftpport",
                             "socks", "socksport", "ignorelist" };

  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  chromeos::ProxyCrosSettingsProvider settings_provider;

  for (size_t i = 0; i < arraysize(settings); ++i) {
    Value* setting = GetProxySetting(settings[i]);
    if (setting)
      return_value->Set(settings[i], setting);
  }

  AutomationJSONReply(this, reply_message).SendSuccess(return_value.get());
}

void TestingAutomationProvider::SetProxySettings(DictionaryValue* args,
                                                IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  std::string key;
  Value* value;
  if (!args->GetString("key", &key) || !args->Get("value", &value)) {
    reply.SendError("Invalid or missing args.");
    return;
  }

  std::string setting_path = "cros.session.proxy.";
  setting_path.append(key);

  // ProxyCrosSettingsProvider will own the Value* passed to Set().
  chromeos::ProxyCrosSettingsProvider().Set(setting_path, value->DeepCopy());
  reply.SendSuccess(NULL);
}

void TestingAutomationProvider::ConnectToWifiNetwork(
    DictionaryValue* args, IPC::Message* reply_message) {
  if (!EnsureCrosLibraryLoaded(this, reply_message))
    return;

  AutomationJSONReply reply(this, reply_message);
  std::string service_path, password, identity, certpath;
  if (!args->GetString("service_path", &service_path) ||
      !args->GetString("password", &password) ||
      !args->GetString("identity", &identity) ||
      !args->GetString("certpath", &certpath)) {
    reply.SendError("Invalid or missing args.");
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
  new ServicePathConnectObserver(this, reply_message, service_path);

  network_library->ConnectToWifiNetwork(wifi);
  network_library->RequestNetworkScan();
}

void TestingAutomationProvider::ConnectToHiddenWifiNetwork(
    DictionaryValue* args, IPC::Message* reply_message) {
  if (!CrosLibrary::Get()->EnsureLoaded()) {
    AutomationJSONReply(this, reply_message)
        .SendError("Could not load cros library.");
    return;
  }

  std::string ssid, security, password, identity, certpath;
  if (!args->GetString("ssid", &ssid) ||
      !args->GetString("security", &security) ||
      !args->GetString("password", &password) ||
      !args->GetString("identity", &identity) ||
      !args->GetString("certpath", &certpath)) {
    AutomationJSONReply(this, reply_message)
        .SendError("Invalid or missing args.");
    return;
  }

  std::map<std::string, chromeos::ConnectionSecurity> connection_security_map;
  connection_security_map["SECURITY_NONE"] = chromeos::SECURITY_NONE;
  connection_security_map["SECURITY_WEP"] = chromeos::SECURITY_WEP;
  connection_security_map["SECURITY_WPA"] = chromeos::SECURITY_WPA;
  connection_security_map["SECURITY_RSN"] = chromeos::SECURITY_RSN;
  connection_security_map["SECURITY_8021X"] = chromeos::SECURITY_8021X;

  if (connection_security_map.find(security) == connection_security_map.end()) {
    AutomationJSONReply(this, reply_message)
        .SendError("Unknown security type.");
    return;
  }
  chromeos::ConnectionSecurity connection_security =
      connection_security_map[security];

  NetworkLibrary* network_library = CrosLibrary::Get()->GetNetworkLibrary();

  // Set up an observer (it will delete itself).
  new SSIDConnectObserver(this, reply_message, ssid);

  network_library->ConnectToWifiNetwork(connection_security, ssid, password,
                                        identity, certpath);
}

void TestingAutomationProvider::DisconnectFromWifiNetwork(
    DictionaryValue* args, IPC::Message* reply_message) {
  if (!EnsureCrosLibraryLoaded(this, reply_message))
    return;

  AutomationJSONReply reply(this, reply_message);
  NetworkLibrary* network_library = CrosLibrary::Get()->GetNetworkLibrary();
  const chromeos::WifiNetwork* wifi = network_library->wifi_network();
  if (!wifi) {
    reply.SendError("Not connected to any wifi network.");
    return;
  }

  network_library->DisconnectFromNetwork(wifi);
  reply.SendSuccess(NULL);
}

void TestingAutomationProvider::GetUpdateInfo(DictionaryValue* args,
                                              IPC::Message* reply_message) {
  if (!EnsureCrosLibraryLoaded(this, reply_message))
    return;

  UpdateLibrary* update_library = CrosLibrary::Get()->GetUpdateLibrary();
  AutomationJSONReply* reply = new AutomationJSONReply(this, reply_message);
  update_library->GetReleaseTrack(GetReleaseTrackCallback, reply);
}

void TestingAutomationProvider::UpdateCheck(
    DictionaryValue* args,
    IPC::Message* reply_message) {
  if (!EnsureCrosLibraryLoaded(this, reply_message))
    return;

  UpdateLibrary* update_library = CrosLibrary::Get()->GetUpdateLibrary();
  AutomationJSONReply* reply = new AutomationJSONReply(this, reply_message);
  update_library->RequestUpdateCheck(UpdateCheckCallback, reply);
}

void TestingAutomationProvider::SetReleaseTrack(DictionaryValue* args,
                                                IPC::Message* reply_message) {
  if (!EnsureCrosLibraryLoaded(this, reply_message))
    return;

  AutomationJSONReply reply(this, reply_message);
  std::string track;
  if (!args->GetString("track", &track)) {
    reply.SendError("Invalid or missing args.");
    return;
  }

  UpdateLibrary* update_library = CrosLibrary::Get()->GetUpdateLibrary();
  update_library->SetReleaseTrack(track);
  reply.SendSuccess(NULL);
}
