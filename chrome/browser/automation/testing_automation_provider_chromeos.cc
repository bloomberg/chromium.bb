// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/testing_automation_provider.h"

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "base/command_line.h"
#include "base/i18n/time_formatting.h"
#include "base/prefs/pref_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/automation/automation_provider_json.h"
#include "chrome/browser/automation/automation_provider_observers.h"
#include "chrome/browser/automation/automation_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/accessibility_util.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/login/default_user_images.h"
#include "chrome/browser/chromeos/login/enrollment/enrollment_screen.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/login_display.h"
#include "chrome/browser/chromeos/login/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/chromeos/login/screens/eula_screen.h"
#include "chrome/browser/chromeos/login/screens/network_screen.h"
#include "chrome/browser/chromeos/login/screens/update_screen.h"
#include "chrome/browser/chromeos/login/screens/user_image_screen.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/webui_login_display.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/net/proxy_config_handler.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/cros_settings_names.h"
#include "chrome/browser/chromeos/system/timezone_settings.h"
#include "chrome/browser/prefs/proxy_config_dictionary.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/pref_names.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/dbus/update_engine_client.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/onc/onc_utils.h"
#include "content/public/browser/web_contents.h"
#include "net/base/network_change_notifier.h"
#include "policy/policy_constants.h"
#include "ui/views/widget/widget.h"

using chromeos::DBusThreadManager;
using chromeos::ExistingUserController;
using chromeos::NetworkLibrary;
using chromeos::UpdateEngineClient;
using chromeos::User;
using chromeos::UserManager;
using chromeos::WizardController;

namespace {

DictionaryValue* GetNetworkInfoDict(const chromeos::Network* network) {
  DictionaryValue* item = new DictionaryValue;
  item->SetString("name", network->name());
  item->SetString("device_path", network->device_path());
  item->SetString("status", network->GetStateString());
  return item;
}

DictionaryValue* GetWifiInfoDict(const chromeos::WifiNetwork* wifi) {
  DictionaryValue* item = GetNetworkInfoDict(wifi);
  item->SetInteger("strength", wifi->strength());
  item->SetBoolean("encrypted", wifi->encrypted());
  item->SetString("encryption", wifi->GetEncryptionString());
  return item;
}

const char* UpdateStatusToString(
    UpdateEngineClient::UpdateStatusOperation status) {
  switch (status) {
    case UpdateEngineClient::UPDATE_STATUS_IDLE:
      return "idle";
    case UpdateEngineClient::UPDATE_STATUS_CHECKING_FOR_UPDATE:
      return "checking for update";
    case UpdateEngineClient::UPDATE_STATUS_UPDATE_AVAILABLE:
      return "update available";
    case UpdateEngineClient::UPDATE_STATUS_DOWNLOADING:
      return "downloading";
    case UpdateEngineClient::UPDATE_STATUS_VERIFYING:
      return "verifying";
    case UpdateEngineClient::UPDATE_STATUS_FINALIZING:
      return "finalizing";
    case UpdateEngineClient::UPDATE_STATUS_UPDATED_NEED_REBOOT:
      return "updated need reboot";
    case UpdateEngineClient::UPDATE_STATUS_REPORTING_ERROR_EVENT:
      return "reporting error event";
    default:
      return "unknown";
  }
}

void UpdateCheckCallback(AutomationJSONReply* reply,
                         UpdateEngineClient::UpdateCheckResult result) {
  if (result == UpdateEngineClient::UPDATE_RESULT_SUCCESS)
    reply->SendSuccess(NULL);
  else
    reply->SendError("update check failed");
  delete reply;
}

const std::string VPNProviderTypeToString(
    chromeos::ProviderType provider_type) {
  switch (provider_type) {
    case chromeos::PROVIDER_TYPE_L2TP_IPSEC_PSK:
      return std::string("L2TP_IPSEC_PSK");
    case chromeos::PROVIDER_TYPE_L2TP_IPSEC_USER_CERT:
      return std::string("L2TP_IPSEC_USER_CERT");
    case chromeos::PROVIDER_TYPE_OPEN_VPN:
      return std::string("OPEN_VPN");
    default:
      return std::string("UNSUPPORTED_PROVIDER_TYPE");
  }
}

}  // namespace

#if defined(OS_CHROMEOS)
void TestingAutomationProvider::PowerChanged(
    const power_manager::PowerSupplyProperties& proto) {
  power_supply_properties_ = proto;
}
#endif

void TestingAutomationProvider::AcceptOOBENetworkScreen(
    DictionaryValue* args,
    IPC::Message* reply_message) {
  WizardController* wizard_controller = WizardController::default_controller();
  if (!wizard_controller || wizard_controller->current_screen()->GetName() !=
          WizardController::kNetworkScreenName) {
    AutomationJSONReply(this, reply_message).SendError(
        "Network screen not active.");
    return;
  }
  // Observer will delete itself.
  new WizardControllerObserver(wizard_controller, this, reply_message);
  wizard_controller->GetNetworkScreen()->OnContinuePressed();
}

void TestingAutomationProvider::AcceptOOBEEula(DictionaryValue* args,
                                               IPC::Message* reply_message) {
  bool accepted;
  bool usage_stats_reporting;
  if (!args->GetBoolean("accepted", &accepted) ||
      !args->GetBoolean("usage_stats_reporting", &usage_stats_reporting)) {
    AutomationJSONReply(this, reply_message).SendError(
        "Invalid or missing args.");
    return;
  }

  WizardController* wizard_controller = WizardController::default_controller();
  if (!wizard_controller || wizard_controller->current_screen()->GetName() !=
      WizardController::kEulaScreenName) {
    AutomationJSONReply(this, reply_message).SendError(
        "EULA screen not active.");
    return;
  }
  // Observer will delete itself.
  new WizardControllerObserver(wizard_controller, this, reply_message);
  wizard_controller->GetEulaScreen()->OnExit(accepted, usage_stats_reporting);
}

void TestingAutomationProvider::CancelOOBEUpdate(DictionaryValue* args,
                                                 IPC::Message* reply_message) {
  if (chromeos::StartupUtils::IsOobeCompleted()) {
    // Update already finished.
    scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
    return_value->SetString("next_screen",
                            WizardController::kLoginScreenName);
    AutomationJSONReply(this, reply_message).SendSuccess(return_value.get());
    return;
  }
  WizardController* wizard_controller = WizardController::default_controller();
  if (!wizard_controller || wizard_controller->current_screen()->GetName() !=
          WizardController::kUpdateScreenName) {
    AutomationJSONReply(this, reply_message).SendError(
        "Update screen not active.");
    return;
  }
  // Observer will delete itself.
  new WizardControllerObserver(wizard_controller, this, reply_message);
  wizard_controller->GetUpdateScreen()->CancelUpdate();
}

void TestingAutomationProvider::GetLoginInfo(DictionaryValue* args,
                                             IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);

  const UserManager* user_manager = UserManager::Get();
  if (!user_manager)
    reply.SendError("No user manager!");
  const chromeos::ScreenLocker* screen_locker =
      chromeos::ScreenLocker::default_screen_locker();

  return_value->SetString("login_ui_type", "webui");
  return_value->SetBoolean("is_owner", user_manager->IsCurrentUserOwner());
  return_value->SetBoolean("is_logged_in", user_manager->IsUserLoggedIn());
  return_value->SetBoolean("is_screen_locked", screen_locker);
  if (user_manager->IsUserLoggedIn()) {
    const User* user = user_manager->GetLoggedInUser();
    return_value->SetBoolean("is_guest", user_manager->IsLoggedInAsGuest());
    return_value->SetString("email", user->email());
    return_value->SetString("display_email", user->display_email());
    switch (user->image_index()) {
      case User::kExternalImageIndex:
        return_value->SetString("user_image", "file");
        break;

      case User::kProfileImageIndex:
        return_value->SetString("user_image", "profile");
        break;

      default:
        return_value->SetInteger("user_image", user->image_index());
        break;
    }
  }

  reply.SendSuccess(return_value.get());
}

// See the note under LoginAsGuest(). CreateAccount() causes a login as guest.
void TestingAutomationProvider::ShowCreateAccountUI(
    DictionaryValue* args, IPC::Message* reply_message) {
  ExistingUserController* controller =
      ExistingUserController::current_controller();
  // Return immediately, since we're going to die before the login is finished.
  AutomationJSONReply(this, reply_message).SendSuccess(NULL);
  controller->CreateAccount();
}

// Logging in as guest will cause session_manager to restart Chrome with new
// flags. If you used EnableChromeTesting, you will have to call it again.
void TestingAutomationProvider::LoginAsGuest(DictionaryValue* args,
                                             IPC::Message* reply_message) {
  ExistingUserController* controller =
      ExistingUserController::current_controller();
  // Return immediately, since we're going to die before the login is finished.
  AutomationJSONReply(this, reply_message).SendSuccess(NULL);
  controller->LoginAsGuest();
}

void TestingAutomationProvider::SubmitLoginForm(DictionaryValue* args,
                                                IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);

  std::string username, password;
  if (!args->GetString("username", &username) ||
      !args->GetString("password", &password)) {
    reply.SendError("Invalid or missing args.");
    return;
  }

  chromeos::ExistingUserController* controller =
      chromeos::ExistingUserController::current_controller();
  if (!controller) {
    reply.SendError("Unable to access ExistingUserController");
    return;
  }

  // WebUI login.
  chromeos::WebUILoginDisplay* webui_login_display =
      static_cast<chromeos::WebUILoginDisplay*>(controller->login_display());
  VLOG(2) << "TestingAutomationProvider::SubmitLoginForm "
          << "ShowSigninScreenForCreds(" << username << ", " << password << ")";

  webui_login_display->ShowSigninScreenForCreds(username, password);
  reply.SendSuccess(NULL);
}

void TestingAutomationProvider::AddLoginEventObserver(
    DictionaryValue* args, IPC::Message* reply_message) {
  ExistingUserController* controller =
      ExistingUserController::current_controller();
  AutomationJSONReply reply(this, reply_message);
  if (!controller) {
    // This may happen due to SkipToLogin not being called.
    reply.SendError("Unable to access ExistingUserController");
    return;
  }

  if (!automation_event_queue_.get())
    automation_event_queue_.reset(new AutomationEventQueue);

  int observer_id = automation_event_queue_->AddObserver(
      new LoginEventObserver(automation_event_queue_.get(), this));

  // Return the observer's id.
  DictionaryValue return_value;
  return_value.SetInteger("observer_id", observer_id);
  reply.SendSuccess(&return_value);
}

void TestingAutomationProvider::SignOut(DictionaryValue* args,
                                        IPC::Message* reply_message) {
  ash::Shell::GetInstance()->system_tray_delegate()->SignOut();
  // Sign out has the side effect of restarting the session_manager
  // and chrome, thereby severing the automation channel, so it's
  // not really necessary to send a reply back. The next line is
  // for consistency with other methods.
  AutomationJSONReply(this, reply_message).SendSuccess(NULL);
}

void TestingAutomationProvider::PickUserImage(DictionaryValue* args,
                                              IPC::Message* reply_message) {
  std::string image_type;
  int image_number = -1;
  if (!args->GetString("image", &image_type)
      && !args->GetInteger("image", &image_number)) {
    AutomationJSONReply(this, reply_message).SendError(
        "Invalid or missing args.");
    return;
  }
  WizardController* wizard_controller = WizardController::default_controller();
  if (!wizard_controller || wizard_controller->current_screen()->GetName() !=
          WizardController::kUserImageScreenName) {
    AutomationJSONReply(this, reply_message).SendError(
        "User image screen not active.");
    return;
  }
  chromeos::UserImageScreen* image_screen =
      wizard_controller->GetUserImageScreen();
  // Observer will delete itself unless error is returned.
  WizardControllerObserver* observer =
      new WizardControllerObserver(wizard_controller, this, reply_message);
  if (image_type == "profile") {
    image_screen->OnImageSelected("", image_type, true);
    image_screen->OnImageAccepted();
  } else if (image_type.empty() && image_number >= 0 &&
             image_number < chromeos::kDefaultImagesCount) {
    image_screen->OnImageSelected(
        chromeos::GetDefaultImageUrl(image_number), image_type, true);
    image_screen->OnImageAccepted();
  } else {
    AutomationJSONReply(this, reply_message).SendError(
        "Invalid or missing args.");
    delete observer;
    return;
  }
}

void TestingAutomationProvider::SkipToLogin(DictionaryValue* args,
                                            IPC::Message* reply_message) {
  bool skip_post_login_screens;
  // The argument name is a legacy. If set to |true|, this argument causes any
  // screens that may otherwise be shown after login (registration, Terms of
  // Service, user image selection) to be skipped.
  if (!args->GetBoolean("skip_image_selection", &skip_post_login_screens)) {
    AutomationJSONReply reply(this, reply_message);
    reply.SendError("Invalid or missing args.");
    return;
  }
  if (skip_post_login_screens)
    WizardController::SkipPostLoginScreensForTesting();

  WizardController* wizard_controller = WizardController::default_controller();
  if (!wizard_controller) {
    AutomationJSONReply reply(this, reply_message);
    if (ExistingUserController::current_controller()) {
      // Already at login screen.
      scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
      return_value->SetString("next_screen",
                              WizardController::kLoginScreenName);
      reply.SendSuccess(return_value.get());
    } else {
      reply.SendError("OOBE not active.");
    }
    return;
  }

  // Observer will delete itself.
  WizardControllerObserver* observer =
      new WizardControllerObserver(wizard_controller, this, reply_message);
  observer->set_screen_to_wait_for(WizardController::kLoginScreenName);
  wizard_controller->SkipToLoginForTesting();
}

void TestingAutomationProvider::GetOOBEScreenInfo(DictionaryValue* args,
                                                  IPC::Message* reply_message) {
  static const char kScreenNameKey[] = "screen_name";
  AutomationJSONReply reply(this, reply_message);
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);

  WizardController* wizard_controller = WizardController::default_controller();
  if (wizard_controller) {
    if (wizard_controller->login_screen_started()) {
      return_value->SetString(kScreenNameKey,
                              WizardController::kLoginScreenName);
    } else {
      return_value->SetString(kScreenNameKey,
                              wizard_controller->current_screen()->GetName());
    }
  } else if (ExistingUserController::current_controller()) {
    return_value->SetString(kScreenNameKey, WizardController::kLoginScreenName);
  } else {
    // Already logged in.
    reply.SendSuccess(NULL);
    return;
  }
  reply.SendSuccess(return_value.get());
}

void TestingAutomationProvider::LockScreen(DictionaryValue* args,
                                           IPC::Message* reply_message) {
  new ScreenLockUnlockObserver(this, reply_message, true);
  DBusThreadManager::Get()->GetSessionManagerClient()->RequestLockScreen();
}

void TestingAutomationProvider::UnlockScreen(DictionaryValue* args,
                                             IPC::Message* reply_message) {
  std::string password;
  if (!args->GetString("password", &password)) {
    AutomationJSONReply(this, reply_message).SendError(
        "Invalid or missing args.");
    return;
  }

  chromeos::ScreenLocker* screen_locker =
      chromeos::ScreenLocker::default_screen_locker();
  if (!screen_locker) {
    AutomationJSONReply(this, reply_message).SendError(
        "No default screen locker. Are you sure the screen is locked?");
    return;
  }

  new ScreenUnlockObserver(this, reply_message);
  screen_locker->AuthenticateByPassword(password);
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
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);

  const bool battery_is_present = power_supply_properties_.battery_state() !=
      power_manager::PowerSupplyProperties_BatteryState_NOT_PRESENT;
  const bool line_power_on = power_supply_properties_.external_power() !=
      power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED;

  return_value->SetBoolean("battery_is_present", battery_is_present);
  return_value->SetBoolean("line_power_on", line_power_on);

  if (battery_is_present) {
    const bool battery_is_full = power_supply_properties_.battery_state() ==
        power_manager::PowerSupplyProperties_BatteryState_FULL;
    return_value->SetBoolean("battery_fully_charged", battery_is_full);
    return_value->SetDouble("battery_percentage",
                            power_supply_properties_.battery_percent());
    if (line_power_on) {
      int64 time = power_supply_properties_.battery_time_to_full_sec();
      if (time > 0 || battery_is_full)
        return_value->SetInteger("battery_seconds_to_full", time);
    } else {
      int64 time = power_supply_properties_.battery_time_to_empty_sec();
      if (time > 0)
        return_value->SetInteger("battery_seconds_to_empty", time);
    }
  }

  AutomationJSONReply(this, reply_message).SendSuccess(return_value.get());
}

void TestingAutomationProvider::GetNetworkInfo(DictionaryValue* args,
                                               IPC::Message* reply_message) {
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  NetworkLibrary* network_library = NetworkLibrary::Get();

  return_value->SetBoolean("offline_mode",
                           net::NetworkChangeNotifier::IsOffline());

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
  return_value->SetBoolean("ethernet_available", ethernet_available);
  return_value->SetBoolean("ethernet_enabled", ethernet_enabled);
  if (ethernet_available && ethernet_enabled) {
    const chromeos::EthernetNetwork* ethernet_network =
        network_library->ethernet_network();
    if (ethernet_network) {
      DictionaryValue* items = new DictionaryValue;
      DictionaryValue* item = GetNetworkInfoDict(ethernet_network);
      items->Set(ethernet_network->service_path(), item);
      items->SetInteger("network_type", chromeos::TYPE_ETHERNET);
      return_value->Set("ethernet_networks", items);
    }
  }

  // Wi-fi networks.
  bool wifi_available = network_library->wifi_available();
  bool wifi_enabled = network_library->wifi_enabled();
  return_value->SetBoolean("wifi_available", wifi_available);
  return_value->SetBoolean("wifi_enabled", wifi_enabled);
  if (wifi_available && wifi_enabled) {
    const chromeos::WifiNetworkVector& wifi_networks =
        network_library->wifi_networks();
    DictionaryValue* items = new DictionaryValue;
    for (chromeos::WifiNetworkVector::const_iterator iter =
         wifi_networks.begin(); iter != wifi_networks.end(); ++iter) {
      const chromeos::WifiNetwork* wifi = *iter;
      DictionaryValue* item = GetWifiInfoDict(wifi);
      items->Set(wifi->service_path(), item);
    }
    items->SetInteger("network_type", chromeos::TYPE_WIFI);
    return_value->Set("wifi_networks", items);
  }

  // Cellular networks.
  bool cellular_available = network_library->cellular_available();
  bool cellular_enabled = network_library->cellular_enabled();
  return_value->SetBoolean("cellular_available", cellular_available);
  return_value->SetBoolean("cellular_enabled", cellular_enabled);
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
      item->SetString("activation_state",
                      cellular_networks[i]->GetActivationStateString());
      item->SetString("roaming_state",
                      cellular_networks[i]->GetRoamingStateString());
      items->Set(cellular_networks[i]->service_path(), item);
    }
    items->SetInteger("network_type", chromeos::TYPE_CELLULAR);
    return_value->Set("cellular_networks", items);
  }

  // Remembered Wifi Networks.
  const chromeos::WifiNetworkVector& remembered_wifi =
      network_library->remembered_wifi_networks();
  DictionaryValue* remembered_wifi_items = new DictionaryValue;
  for (chromeos::WifiNetworkVector::const_iterator iter =
       remembered_wifi.begin(); iter != remembered_wifi.end();
       ++iter) {
      const chromeos::WifiNetwork* wifi = *iter;
      DictionaryValue* item = GetWifiInfoDict(wifi);
      remembered_wifi_items->Set(wifi->service_path(), item);
  }
  remembered_wifi_items->SetInteger("network_type", chromeos::TYPE_WIFI);
  return_value->Set("remembered_wifi", remembered_wifi_items);

  AutomationJSONReply(this, reply_message).SendSuccess(return_value.get());
}

void TestingAutomationProvider::NetworkScan(DictionaryValue* args,
                                            IPC::Message* reply_message) {
  NetworkLibrary* network_library = NetworkLibrary::Get();
  network_library->RequestNetworkScan();

  // Set up an observer (it will delete itself).
  new NetworkScanObserver(this, reply_message);
}

void TestingAutomationProvider::ToggleNetworkDevice(
    DictionaryValue* args, IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  std::string device;
  bool enable;
  if (!args->GetString("device", &device) ||
      !args->GetBoolean("enable", &enable)) {
    reply.SendError("Invalid or missing args.");
    return;
  }

  // Set up an observer (it will delete itself).
  new ToggleNetworkDeviceObserver(this, reply_message, device, enable);

  NetworkLibrary* network_library = NetworkLibrary::Get();
  if (device == "ethernet") {
    network_library->EnableEthernetNetworkDevice(enable);
  } else if (device == "wifi") {
    network_library->EnableWifiNetworkDevice(enable);
  } else if (device == "cellular") {
    network_library->EnableCellularNetworkDevice(enable);
  } else {
    reply.SendError(
        "Unknown device. Valid devices are ethernet, wifi, cellular.");
    return;
  }
}

void TestingAutomationProvider::ForgetWifiNetwork(
    DictionaryValue* args, IPC::Message* reply_message) {
  std::string service_path;
  if (!args->GetString("service_path", &service_path)) {
    AutomationJSONReply(this, reply_message).SendError(
        "Invalid or missing args.");
    return;
  }

  NetworkLibrary::Get()->ForgetNetwork(service_path);
  AutomationJSONReply(this, reply_message).SendSuccess(NULL);
}

void TestingAutomationProvider::ConnectToHiddenWifiNetwork(
    DictionaryValue* args, IPC::Message* reply_message) {
  std::string ssid, security, password;
  bool shared;
  if (!args->GetString("ssid", &ssid) ||
      !args->GetString("security", &security) ||
      !args->GetString("password", &password) ||
      !args->GetBoolean("shared", &shared)) {
    AutomationJSONReply(this, reply_message).SendError(
        "Invalid or missing args.");
    return;
  }

  std::map<std::string, chromeos::ConnectionSecurity> connection_security_map;
  connection_security_map["SECURITY_NONE"] = chromeos::SECURITY_NONE;
  connection_security_map["SECURITY_WEP"] = chromeos::SECURITY_WEP;
  connection_security_map["SECURITY_WPA"] = chromeos::SECURITY_WPA;
  connection_security_map["SECURITY_RSN"] = chromeos::SECURITY_RSN;
  connection_security_map["SECURITY_8021X"] = chromeos::SECURITY_8021X;

  if (connection_security_map.find(security) == connection_security_map.end()) {
    AutomationJSONReply(this, reply_message).SendError(
        "Unknown security type.");
    return;
  }
  chromeos::ConnectionSecurity connection_security =
      connection_security_map[security];

  NetworkLibrary* network_library = NetworkLibrary::Get();

  // Set up an observer (it will delete itself).
  new SSIDConnectObserver(this, reply_message, ssid);

  bool save_credentials = false;

  if (connection_security == chromeos::SECURITY_8021X) {
    chromeos::NetworkLibrary::EAPConfigData config_data;
    std::string eap_method, eap_auth, eap_identity;
    if (!args->GetString("eap_method", &eap_method) ||
        !args->GetString("eap_auth", &eap_auth) ||
        !args->GetString("eap_identity", &eap_identity) ||
        !args->GetBoolean("save_credentials", &save_credentials)) {
      AutomationJSONReply(this, reply_message).SendError(
          "Invalid or missing EAP args.");
      return;
    }

    std::map<std::string, chromeos::EAPMethod> eap_method_map;
    eap_method_map["EAP_METHOD_NONE"] = chromeos::EAP_METHOD_UNKNOWN;
    eap_method_map["EAP_METHOD_PEAP"] = chromeos::EAP_METHOD_PEAP;
    eap_method_map["EAP_METHOD_TLS"] = chromeos::EAP_METHOD_TLS;
    eap_method_map["EAP_METHOD_TTLS"] = chromeos::EAP_METHOD_TTLS;
    eap_method_map["EAP_METHOD_LEAP"] = chromeos::EAP_METHOD_LEAP;
    if (eap_method_map.find(eap_method) == eap_method_map.end()) {
      AutomationJSONReply(this, reply_message).SendError(
          "Unknown EAP Method type.");
      return;
    }
    config_data.method = eap_method_map[eap_method];

    std::map<std::string, chromeos::EAPPhase2Auth> eap_auth_map;
    eap_auth_map["EAP_PHASE_2_AUTH_AUTO"] = chromeos::EAP_PHASE_2_AUTH_AUTO;
    eap_auth_map["EAP_PHASE_2_AUTH_MD5"] = chromeos::EAP_PHASE_2_AUTH_MD5;
    eap_auth_map["EAP_PHASE_2_AUTH_MSCHAP"] =
        chromeos::EAP_PHASE_2_AUTH_MSCHAP;
    eap_auth_map["EAP_PHASE_2_AUTH_MSCHAPV2"] =
        chromeos::EAP_PHASE_2_AUTH_MSCHAPV2;
    eap_auth_map["EAP_PHASE_2_AUTH_PAP"] = chromeos::EAP_PHASE_2_AUTH_PAP;
    eap_auth_map["EAP_PHASE_2_AUTH_CHAP"] = chromeos::EAP_PHASE_2_AUTH_CHAP;
    if (eap_auth_map.find(eap_auth) == eap_auth_map.end()) {
      AutomationJSONReply(this, reply_message).SendError(
          "Unknown EAP Phase2 Auth type.");
      return;
    }
    config_data.auth = eap_auth_map[eap_auth];

    config_data.identity = eap_identity;

    // TODO(stevenjb): Parse cert values?
    config_data.use_system_cas = false;
    config_data.client_cert_pkcs11_id = "";

    network_library->ConnectToUnconfiguredWifiNetwork(
        ssid, chromeos::SECURITY_8021X, password, &config_data,
        save_credentials, shared);
  } else {
    network_library->ConnectToUnconfiguredWifiNetwork(
        ssid, connection_security, password, NULL,
        save_credentials, shared);
  }
}

void TestingAutomationProvider::ExecuteJavascriptInOOBEWebUI(
    DictionaryValue* args, IPC::Message* reply_message) {
  std::string javascript, frame_xpath;
  if (!args->GetString("javascript", &javascript)) {
    AutomationJSONReply(this, reply_message)
        .SendError("'javascript' missing or invalid");
    return;
  }
  if (!args->GetString("frame_xpath", &frame_xpath)) {
    AutomationJSONReply(this, reply_message)
        .SendError("'frame_xpath' missing or invalid");
    return;
  }
  const UserManager* user_manager = UserManager::Get();
  if (!user_manager) {
    AutomationJSONReply(this, reply_message).SendError(
        "No user manager!");
    return;
  }
  if (user_manager->IsUserLoggedIn()) {
    AutomationJSONReply(this, reply_message).SendError(
        "User is already logged in.");
    return;
  }
  ExistingUserController* controller =
      ExistingUserController::current_controller();
  if (!controller) {
    AutomationJSONReply(this, reply_message).SendError(
        "Unable to access ExistingUserController");
    return;
  }
  chromeos::LoginDisplayHostImpl* webui_host =
      static_cast<chromeos::LoginDisplayHostImpl*>(
          controller->login_display_host());
  content::WebContents* web_contents =
      webui_host->GetOobeUI()->web_ui()->GetWebContents();

  new DomOperationMessageSender(this, reply_message, true);
  ExecuteJavascriptInRenderViewFrame(ASCIIToUTF16(frame_xpath),
                                     ASCIIToUTF16(javascript),
                                     reply_message,
                                     web_contents->GetRenderViewHost());
}

void TestingAutomationProvider::EnableSpokenFeedback(
    DictionaryValue* args, IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  bool enabled;
  if (!args->GetBoolean("enabled", &enabled)) {
    reply.SendError("Invalid or missing args.");
    return;
  }
  chromeos::AccessibilityManager::Get()->EnableSpokenFeedback(
      enabled, ash::A11Y_NOTIFICATION_NONE);

  reply.SendSuccess(return_value.get());
}

void TestingAutomationProvider::IsSpokenFeedbackEnabled(
    DictionaryValue* args, IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  return_value->SetBoolean(
      "spoken_feedback",
      chromeos::AccessibilityManager::Get()->IsSpokenFeedbackEnabled());
  reply.SendSuccess(return_value.get());
}

void TestingAutomationProvider::GetTimeInfo(Browser* browser,
                                            DictionaryValue* args,
                                            IPC::Message* reply_message) {
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  base::Time time(base::Time::Now());
  bool use_24hour_clock = browser && browser->profile()->GetPrefs()->GetBoolean(
      prefs::kUse24HourClock);
  base::HourClockType hour_clock_type =
      use_24hour_clock ? base::k24HourClock : base::k12HourClock;
  string16 display_time = base::TimeFormatTimeOfDayWithHourClockType(
      time, hour_clock_type, base::kDropAmPm);
  string16 timezone =
      chromeos::system::TimezoneSettings::GetInstance()->GetCurrentTimezoneID();
  return_value->SetString("display_time", display_time);
  return_value->SetString("display_date", base::TimeFormatFriendlyDate(time));
  return_value->SetString("timezone", timezone);
  AutomationJSONReply(this, reply_message).SendSuccess(return_value.get());
}

void TestingAutomationProvider::GetTimeInfo(DictionaryValue* args,
                                            IPC::Message* reply_message) {
  GetTimeInfo(NULL, args, reply_message);
}

void TestingAutomationProvider::SetTimezone(DictionaryValue* args,
                                            IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  std::string timezone_id;
  if (!args->GetString("timezone", &timezone_id)) {
    reply.SendError("Invalid or missing args.");
    return;
  }
  chromeos::CrosSettings* settings = chromeos::CrosSettings::Get();
  settings->SetString(chromeos::kSystemTimezone, timezone_id);
  reply.SendSuccess(NULL);
}

void TestingAutomationProvider::UpdateCheck(
    DictionaryValue* args,
    IPC::Message* reply_message) {
  AutomationJSONReply* reply = new AutomationJSONReply(this, reply_message);
  DBusThreadManager::Get()->GetUpdateEngineClient()
      ->RequestUpdateCheck(base::Bind(UpdateCheckCallback, reply));
}

void TestingAutomationProvider::GetVolumeInfo(DictionaryValue* args,
                                              IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  chromeos::CrasAudioHandler* audio_handler = chromeos::CrasAudioHandler::Get();
  if (!audio_handler) {
    reply.SendError("CrasAudioHandler not initialized.");
    return;
  }
  return_value->SetDouble("volume", audio_handler->GetOutputVolumePercent());
  return_value->SetBoolean("is_mute", audio_handler->IsOutputMuted());
  reply.SendSuccess(return_value.get());
}

void TestingAutomationProvider::SetVolume(DictionaryValue* args,
                                          IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  double volume_percent;
  if (!args->GetDouble("volume", &volume_percent)) {
    reply.SendError("Invalid or missing args.");
    return;
  }
  chromeos::CrasAudioHandler* audio_handler = chromeos::CrasAudioHandler::Get();
  if (!audio_handler) {
    reply.SendError("CrasAudioHandler not initialized.");
    return;
  }
  audio_handler->SetOutputVolumePercent(volume_percent);
  reply.SendSuccess(NULL);
}

void TestingAutomationProvider::SetMute(DictionaryValue* args,
                                        IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  bool mute;
  if (!args->GetBoolean("mute", &mute)) {
    reply.SendError("Invalid or missing args.");
    return;
  }
  chromeos::CrasAudioHandler* audio_handler = chromeos::CrasAudioHandler::Get();
  if (!audio_handler) {
    reply.SendError("CrasAudioHandler not initialized.");
    return;
  }
  audio_handler->SetOutputMute(mute);
  reply.SendSuccess(NULL);
}

void TestingAutomationProvider::OpenCrosh(DictionaryValue* args,
                                          IPC::Message* reply_message) {
  new NavigationNotificationObserver(
      NULL, this, reply_message, 1, false, true);
  ash::Shell::GetInstance()->delegate()->OpenCrosh();
}

void TestingAutomationProvider::AddChromeosObservers() {
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
      AddObserver(this);
}

void TestingAutomationProvider::RemoveChromeosObservers() {
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
      RemoveObserver(this);
}
