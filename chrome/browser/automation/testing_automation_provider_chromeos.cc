// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/testing_automation_provider.h"

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "base/command_line.h"
#include "base/i18n/time_formatting.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/automation/automation_provider_json.h"
#include "chrome/browser/automation/automation_provider_observers.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/accessibility/accessibility_util.h"
#include "chrome/browser/chromeos/audio/audio_handler.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/login/default_user_images.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_screen.h"
#include "chrome/browser/chromeos/login/eula_screen.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/login_display.h"
#include "chrome/browser/chromeos/login/login_display_host.h"
#include "chrome/browser/chromeos/login/network_screen.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/chromeos/login/update_screen.h"
#include "chrome/browser/chromeos/login/user_image_screen.h"
#include "chrome/browser/chromeos/login/webui_login_display.h"
#include "chrome/browser/chromeos/login/webui_login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/options/take_photo_dialog.h"
#include "chrome/browser/chromeos/proxy_cros_settings_parser.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/cros_settings_names.h"
#include "chrome/browser/chromeos/system/timezone_settings.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/cloud_policy_cache_base.h"
#include "chrome/browser/policy/cloud_policy_data_store.h"
#include "chrome/browser/policy/cloud_policy_subsystem.h"
#include "chrome/browser/policy/enterprise_install_attributes.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/dbus/update_engine_client.h"
#include "content/public/browser/web_contents.h"
#include "net/base/network_change_notifier.h"
#include "policy/policy_constants.h"
#include "ui/views/widget/widget.h"

using chromeos::CrosLibrary;
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
  item->SetString("ip_address", network->ip_address());
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

base::Value* GetProxySetting(Browser* browser,
                             const std::string& setting_name) {
  std::string setting_path = "cros.session.proxy.";
  setting_path.append(setting_name);

  base::Value* setting;
  if (chromeos::proxy_cros_settings_parser::GetProxyPrefValue(
          browser->profile(), setting_path, &setting)) {
    scoped_ptr<DictionaryValue> setting_dict(
        static_cast<DictionaryValue*>(setting));
    base::Value* value;
    if (setting_dict->Remove("value", &value))
      return value;
  }

  return NULL;
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

void GetReleaseTrackCallback(AutomationJSONReply* reply,
                             const std::string& track) {
  if (track.empty()) {
    reply->SendError("Unable to get release track.");
    delete reply;
    return;
  }

  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  return_value->SetString("release_track", track);

  const UpdateEngineClient::Status& status =
      DBusThreadManager::Get()->GetUpdateEngineClient()->GetLastStatus();
  UpdateEngineClient::UpdateStatusOperation update_status =
      status.status;
  return_value->SetString("status", UpdateStatusToString(update_status));
  if (update_status == UpdateEngineClient::UPDATE_STATUS_DOWNLOADING)
    return_value->SetDouble("download_progress", status.download_progress);
  if (status.last_checked_time > 0)
    return_value->SetInteger("last_checked_time", status.last_checked_time);
  if (status.new_size > 0)
    return_value->SetInteger("new_size", status.new_size);

  reply->SendSuccess(return_value.get());
  delete reply;
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

const char* EnterpriseStatusToString(
    policy::CloudPolicySubsystem::PolicySubsystemState state) {
  switch (state) {
    case policy::CloudPolicySubsystem::UNENROLLED:
      return "UNENROLLED";
    case policy::CloudPolicySubsystem::BAD_GAIA_TOKEN:
      return "BAD_GAIA_TOKEN";
    case policy::CloudPolicySubsystem::UNMANAGED:
      return "UNMANAGED";
    case policy::CloudPolicySubsystem::NETWORK_ERROR:
      return "NETWORK_ERROR";
    case policy::CloudPolicySubsystem::LOCAL_ERROR:
      return "LOCAL_ERROR";
    case policy::CloudPolicySubsystem::TOKEN_FETCHED:
      return "TOKEN_FETCHED";
    case policy::CloudPolicySubsystem::SUCCESS:
      return "SUCCESS";
    default:
      return "UNKNOWN_STATE";
  }
}

// Fills the supplied DictionaryValue with all policy settings held by the
// given CloudPolicySubsystem (Device or User subsystems) at the given
// PolicyLevel (Mandatory or Recommended policies).
DictionaryValue* CreateDictionaryWithPolicies(
    policy::CloudPolicySubsystem* policy_subsystem,
    policy::PolicyLevel policy_level) {
  DictionaryValue* dict = new DictionaryValue;
  policy::CloudPolicyCacheBase* policy_cache =
      policy_subsystem->GetCloudPolicyCacheBase();
  if (policy_cache) {
    const policy::PolicyMap* policy_map = policy_cache->policy();
    if (policy_map) {
      policy::PolicyMap::const_iterator i;
      for (i = policy_map->begin(); i != policy_map->end(); i++) {
        if (i->second.level == policy_level)
          dict->Set(i->first, i->second.value->DeepCopy());
      }
    }
  }
  return dict;
}

// Last reported power status.
chromeos::PowerSupplyStatus global_power_status;

}  // namespace

class PowerManagerClientObserverForTesting
    : public chromeos::PowerManagerClient::Observer {
 public:
  virtual ~PowerManagerClientObserverForTesting() {}

  virtual void PowerChanged(const chromeos::PowerSupplyStatus& status)
      OVERRIDE {
    global_power_status = status;
  }
};

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
  WizardController* wizard_controller = WizardController::default_controller();
  if (wizard_controller && wizard_controller->IsOobeCompleted()) {
    // Update already finished.
    scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
    return_value->SetString("next_screen",
                            WizardController::kLoginScreenName);
    AutomationJSONReply(this, reply_message).SendSuccess(return_value.get());
    return;
  }
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
    const User& user = user_manager->GetLoggedInUser();
    return_value->SetBoolean("is_guest", user_manager->IsLoggedInAsGuest());
    return_value->SetString("email", user.email());
    return_value->SetString("display_email", user.display_email());
    switch (user.image_index()) {
      case User::kExternalImageIndex:
        return_value->SetString("user_image", "file");
        break;

      case User::kProfileImageIndex:
        return_value->SetString("user_image", "profile");
        break;

      default:
        return_value->SetInteger("user_image", user.image_index());
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
      new LoginEventObserver(automation_event_queue_.get(),
                             controller, this));

  // Return the observer's id.
  DictionaryValue return_value;
  return_value.SetInteger("observer_id", observer_id);
  reply.SendSuccess(&return_value);
}

void TestingAutomationProvider::SignOut(DictionaryValue* args,
                                        IPC::Message* reply_message) {
  ash::Shell::GetInstance()->tray_delegate()->SignOut();
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
    image_screen->OnProfileImageSelected();
  } else if (image_type.empty() && image_number >= 0 &&
             image_number < chromeos::kDefaultImagesCount) {
    image_screen->OnDefaultImageSelected(image_number);
  } else {
    AutomationJSONReply(this, reply_message).SendError(
        "Invalid or missing args.");
    delete observer;
    return;
  }
}

void TestingAutomationProvider::SkipToLogin(DictionaryValue* args,
                                            IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);

  bool skip_image_selection;
  if (!args->GetBoolean("skip_image_selection", &skip_image_selection)) {
    reply.SendError("Invalid or missing args.");
    return;
  }
  if (skip_image_selection)
    WizardController::SkipImageSelectionForTesting();

  WizardController* wizard_controller = WizardController::default_controller();
  if (!wizard_controller) {
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
  screen_locker->Authenticate(ASCIIToUTF16(password));
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

  return_value->SetBoolean("battery_is_present",
                           global_power_status.battery_is_present);
  return_value->SetBoolean("line_power_on", global_power_status.line_power_on);
  if (global_power_status.battery_is_present) {
    return_value->SetBoolean("battery_fully_charged",
                             global_power_status.battery_is_full);
    return_value->SetDouble("battery_percentage",
                            global_power_status.battery_percentage);
    if (global_power_status.line_power_on) {
      int64 time = global_power_status.battery_seconds_to_full;
      if (time > 0 || global_power_status.battery_is_full)
        return_value->SetInteger("battery_seconds_to_full", time);
    } else {
      int64 time = global_power_status.battery_seconds_to_empty;
      if (time > 0)
        return_value->SetInteger("battery_seconds_to_empty", time);
    }
  }

  AutomationJSONReply(this, reply_message).SendSuccess(return_value.get());
}

void TestingAutomationProvider::GetNetworkInfo(DictionaryValue* args,
                                               IPC::Message* reply_message) {
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  NetworkLibrary* network_library = CrosLibrary::Get()->GetNetworkLibrary();

  return_value->SetBoolean("offline_mode",
                           net::NetworkChangeNotifier::IsOffline());

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
  return_value->SetBoolean("ethernet_available", ethernet_available);
  return_value->SetBoolean("ethernet_enabled", ethernet_enabled);
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
  return_value->Set("remembered_wifi", remembered_wifi_items);

  AutomationJSONReply(this, reply_message).SendSuccess(return_value.get());
}

void TestingAutomationProvider::NetworkScan(DictionaryValue* args,
                                            IPC::Message* reply_message) {
  NetworkLibrary* network_library = CrosLibrary::Get()->GetNetworkLibrary();
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

  NetworkLibrary* network_library = CrosLibrary::Get()->GetNetworkLibrary();
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

void TestingAutomationProvider::GetProxySettings(Browser* browser,
                                                 DictionaryValue* args,
                                                 IPC::Message* reply_message) {
  const char* settings[] = { "pacurl", "singlehttp", "singlehttpport",
                             "httpurl", "httpport", "httpsurl", "httpsport",
                             "type", "single", "ftpurl", "ftpport",
                             "socks", "socksport", "ignorelist" };

  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);

  for (size_t i = 0; i < arraysize(settings); ++i) {
    base::Value* setting = GetProxySetting(browser, settings[i]);
    if (setting)
      return_value->Set(settings[i], setting);
  }

  AutomationJSONReply(this, reply_message).SendSuccess(return_value.get());
}

void TestingAutomationProvider::SetProxySettings(Browser* browser,
                                                 DictionaryValue* args,
                                                 IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  std::string key;
  base::Value* value;
  if (!args->GetString("key", &key) || !args->Get("value", &value)) {
    reply.SendError("Invalid or missing args.");
    return;
  }

  std::string setting_path = "cros.session.proxy.";
  setting_path.append(key);

  // ProxyCrosSettingsProvider will own the Value* passed to Set().
  chromeos::proxy_cros_settings_parser::SetProxyPrefValue(
      browser->profile(), setting_path, value);
  reply.SendSuccess(NULL);
}

void TestingAutomationProvider::ConnectToCellularNetwork(
    DictionaryValue* args, IPC::Message* reply_message) {
  std::string service_path;
  if (!args->GetString("service_path", &service_path)) {
    AutomationJSONReply(this, reply_message).SendError(
        "Invalid or missing args.");
    return;
  }

  NetworkLibrary* network_library = CrosLibrary::Get()->GetNetworkLibrary();
  chromeos::CellularNetwork* cellular =
      network_library->FindCellularNetworkByPath(service_path);
  if (!cellular) {
    AutomationJSONReply(this, reply_message).SendError(
        "No network found with specified service path.");
    return;
  }

  // Set up an observer (it will delete itself).
  new ServicePathConnectObserver(this, reply_message, service_path);

  network_library->ConnectToCellularNetwork(cellular);
  network_library->RequestNetworkScan();
}

void TestingAutomationProvider::DisconnectFromCellularNetwork(
    DictionaryValue* args, IPC::Message* reply_message) {
  NetworkLibrary* network_library = CrosLibrary::Get()->GetNetworkLibrary();
  const chromeos::CellularNetwork* cellular =
        network_library->cellular_network();
  if (!cellular) {
    AutomationJSONReply(this, reply_message).SendError(
        "Not connected to any cellular network.");
    return;
  }

  // Set up an observer (it will delete itself).
  new NetworkDisconnectObserver(this, reply_message, cellular->service_path());

  network_library->DisconnectFromNetwork(cellular);
}

void TestingAutomationProvider::ConnectToWifiNetwork(
    DictionaryValue* args, IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  std::string service_path, password;
  bool shared;
  if (!args->GetString("service_path", &service_path) ||
      !args->GetString("password", &password) ||
      !args->GetBoolean("shared", &shared)) {
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

  // Regardless of what was passed, if the network is open and visible,
  // the network must be shared because of a UI restriction.
  if (wifi->encryption() == chromeos::SECURITY_NONE)
    shared = true;

  // Set up an observer (it will delete itself).
  new ServicePathConnectObserver(this, reply_message, service_path);

  network_library->ConnectToWifiNetwork(wifi, shared);
  network_library->RequestNetworkScan();
}

void TestingAutomationProvider::ForgetWifiNetwork(
    DictionaryValue* args, IPC::Message* reply_message) {
  std::string service_path;
  if (!args->GetString("service_path", &service_path)) {
    AutomationJSONReply(this, reply_message).SendError(
        "Invalid or missing args.");
    return;
  }

  CrosLibrary::Get()->GetNetworkLibrary()->ForgetNetwork(service_path);
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

  NetworkLibrary* network_library = CrosLibrary::Get()->GetNetworkLibrary();

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
    config_data.server_ca_cert_nss_nickname = "";
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

void TestingAutomationProvider::DisconnectFromWifiNetwork(
    DictionaryValue* args, IPC::Message* reply_message) {
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

void TestingAutomationProvider::AddPrivateNetwork(
    DictionaryValue* args, IPC::Message* reply_message) {
  std::string hostname, service_name, provider_type, key, cert_id, cert_nss,
      username, password;
  if (!args->GetString("hostname", &hostname) ||
      !args->GetString("service_name", &service_name) ||
      !args->GetString("provider_type", &provider_type) ||
      !args->GetString("username", &username) ||
      !args->GetString("password", &password)) {
    AutomationJSONReply(this, reply_message)
        .SendError("Invalid or missing args.");
    return;
  }

  NetworkLibrary* network_library = CrosLibrary::Get()->GetNetworkLibrary();

  // Attempt to connect to the VPN based on the provider type.
  if (provider_type == VPNProviderTypeToString(
      chromeos::PROVIDER_TYPE_L2TP_IPSEC_PSK)) {
    if (!args->GetString("key", &key)) {
      AutomationJSONReply(this, reply_message)
          .SendError("Missing key arg.");
      return;
    }
    new VirtualConnectObserver(this, reply_message, service_name);
    // Connect using a pre-shared key.
    chromeos::NetworkLibrary::VPNConfigData config_data;
    config_data.psk = key;
    config_data.username = username;
    config_data.user_passphrase = password;
    network_library->ConnectToUnconfiguredVirtualNetwork(
        service_name,
        hostname,
        chromeos::PROVIDER_TYPE_L2TP_IPSEC_PSK,
        config_data);
  } else if (provider_type == VPNProviderTypeToString(
      chromeos::PROVIDER_TYPE_L2TP_IPSEC_USER_CERT)) {
    if (!args->GetString("cert_id", &cert_id) ||
        !args->GetString("cert_nss", &cert_nss)) {
      AutomationJSONReply(this, reply_message)
          .SendError("Missing a certificate arg.");
      return;
    }
    new VirtualConnectObserver(this, reply_message, service_name);
    // Connect using a user certificate.
    chromeos::NetworkLibrary::VPNConfigData config_data;
    config_data.server_ca_cert_nss_nickname = cert_nss;
    config_data.client_cert_pkcs11_id = cert_id;
    config_data.username = username;
    config_data.user_passphrase = password;
    network_library->ConnectToUnconfiguredVirtualNetwork(
        service_name,
        hostname,
        chromeos::PROVIDER_TYPE_L2TP_IPSEC_USER_CERT,
        config_data);
  } else if (provider_type == VPNProviderTypeToString(
      chromeos::PROVIDER_TYPE_OPEN_VPN)) {
    std::string otp;
    args->GetString("otp", &otp);
    // Connect using OPEN_VPN.
    chromeos::NetworkLibrary::VPNConfigData config_data;
    config_data.server_ca_cert_nss_nickname = cert_nss;
    config_data.client_cert_pkcs11_id = cert_id;
    config_data.username = username;
    config_data.user_passphrase = password;
    config_data.otp = otp;
    network_library->ConnectToUnconfiguredVirtualNetwork(
        service_name,
        hostname,
        chromeos::PROVIDER_TYPE_OPEN_VPN,
        config_data);
  } else {
    AutomationJSONReply(this, reply_message)
        .SendError("Unsupported provider type.");
    return;
  }
}

void TestingAutomationProvider::ConnectToPrivateNetwork(
    DictionaryValue* args, IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  std::string service_path;
  if (!args->GetString("service_path", &service_path)) {
    reply.SendError("Invalid or missing args.");
    return;
  }

  // Connect to a remembered VPN by its service_path. Valid service_paths
  // can be found in the dictionary returned by GetPrivateNetworkInfo.
  NetworkLibrary* network_library = CrosLibrary::Get()->GetNetworkLibrary();
  chromeos::VirtualNetwork* network =
      network_library->FindVirtualNetworkByPath(service_path);
  if (!network) {
    reply.SendError(StringPrintf("No virtual network found: %s",
                                 service_path.c_str()));
    return;
  }
  if (network->NeedMoreInfoToConnect()) {
    reply.SendError("Virtual network is missing info required to connect.");
    return;
  };

  // Set up an observer (it will delete itself).
  new VirtualConnectObserver(this, reply_message, network->name());
  network_library->ConnectToVirtualNetwork(network);
}

void TestingAutomationProvider::GetPrivateNetworkInfo(
    DictionaryValue* args, IPC::Message* reply_message) {
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  NetworkLibrary* network_library = CrosLibrary::Get()->GetNetworkLibrary();
  const chromeos::VirtualNetworkVector& virtual_networks =
      network_library->virtual_networks();

  // Construct a dictionary of fields describing remembered VPNs. Also list
  // the currently active VPN, if any.
  if (network_library->virtual_network())
    return_value->SetString("connected",
                            network_library->virtual_network()->service_path());
  for (chromeos::VirtualNetworkVector::const_iterator iter =
       virtual_networks.begin(); iter != virtual_networks.end(); ++iter) {
    const chromeos::VirtualNetwork* virt = *iter;
    DictionaryValue* item = new DictionaryValue;
    item->SetString("name", virt->name());
    item->SetString("provider_type",
                    VPNProviderTypeToString(virt->provider_type()));
    item->SetString("hostname", virt->server_hostname());
    item->SetString("key", virt->psk_passphrase());
    item->SetString("cert_nss", virt->ca_cert_nss());
    item->SetString("cert_id", virt->client_cert_id());
    item->SetString("username", virt->username());
    item->SetString("password", virt->user_passphrase());
    return_value->Set(virt->service_path(), item);
  }

  AutomationJSONReply(this, reply_message).SendSuccess(return_value.get());
}

void TestingAutomationProvider::DisconnectFromPrivateNetwork(
    DictionaryValue* args, IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  NetworkLibrary* network_library = CrosLibrary::Get()->GetNetworkLibrary();
  const chromeos::VirtualNetwork* virt = network_library->virtual_network();
  if (!virt) {
    reply.SendError("Not connected to any virtual network.");
    return;
  }

  network_library->DisconnectFromNetwork(virt);
  reply.SendSuccess(NULL);
}

void TestingAutomationProvider::IsEnterpriseDevice(
    DictionaryValue* args, IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  policy::BrowserPolicyConnector* connector =
      g_browser_process->browser_policy_connector();
  if (!connector) {
    reply.SendError("Unable to access BrowserPolicyConnector");
    return;
  }
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  return_value->SetBoolean("enterprise", connector->IsEnterpriseManaged());
  reply.SendSuccess(return_value.get());
}

void TestingAutomationProvider::EnrollEnterpriseDevice(
    DictionaryValue* args, IPC::Message* reply_message) {
  std::string user, password;
  if (!args->GetString("user", &user) ||
      !args->GetString("password", &password)) {
    AutomationJSONReply(this, reply_message)
        .SendError("Invalid or missing args.");
    return;
  }
  ExistingUserController* user_controller =
      ExistingUserController::current_controller();
  if (!user_controller) {
    AutomationJSONReply(this, reply_message).SendError(
        "Unable to access ExistingUserController");
    return;
  }
  user_controller->login_display_host()->StartWizard(
      chromeos::WizardController::kEnterpriseEnrollmentScreenName,
      NULL);
  WizardController* wizard_controller = WizardController::default_controller();
  if (!wizard_controller) {
    AutomationJSONReply(this, reply_message).SendError(
        "Unable to access WizardController");
    return;
  }
  chromeos::EnterpriseEnrollmentScreen* enroll_screen =
      wizard_controller->GetEnterpriseEnrollmentScreen();
  if (!enroll_screen) {
    AutomationJSONReply(this, reply_message).SendError(
        "Unable to access EnterpriseEnrollmentScreen");
    return;
  }
  // Set up an observer (it will delete itself).
  new EnrollmentObserver(this, reply_message, enroll_screen->GetActor(),
                         enroll_screen);
  enroll_screen->GetActor()->SubmitTestCredentials(user, password);
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
  chromeos::WebUILoginDisplayHost* webui_login_display_host =
      static_cast<chromeos::WebUILoginDisplayHost*>(
          controller->login_display_host());
  content::WebContents* web_contents =
      webui_login_display_host->GetOobeUI()->web_ui()->GetWebContents();

  new DomOperationMessageSender(this, reply_message, true);
  ExecuteJavascriptInRenderViewFrame(ASCIIToUTF16(frame_xpath),
                                     ASCIIToUTF16(javascript),
                                     reply_message,
                                     web_contents->GetRenderViewHost());
}

void TestingAutomationProvider::GetEnterprisePolicyInfo(
    DictionaryValue* args, IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);

  policy::BrowserPolicyConnector* connector =
      g_browser_process->browser_policy_connector();
  if (!connector) {
    reply.SendError("Unable to access BrowserPolicyConnector");
    return;
  }
  policy::CloudPolicySubsystem* user_cloud_policy =
      connector->user_cloud_policy_subsystem();
  policy::CloudPolicySubsystem* device_cloud_policy =
      connector->device_cloud_policy_subsystem();
  const policy::CloudPolicyDataStore* user_data_store =
      connector->GetUserCloudPolicyDataStore();
  const policy::CloudPolicyDataStore* device_data_store =
      connector->GetDeviceCloudPolicyDataStore();
  if (!user_cloud_policy || !device_cloud_policy) {
    reply.SendError("Unable to access a CloudPolicySubsystem");
    return;
  }
  if (!user_data_store || !device_data_store) {
    reply.SendError("Unable to access a CloudPolicyDataStore");
    return;
  }

  // Get various policy related fields.
  return_value->SetString("enterprise_domain",
                          connector->GetEnterpriseDomain());
  return_value->SetString("user_cloud_policy",
      EnterpriseStatusToString(user_cloud_policy->state()));
  return_value->SetString("device_cloud_policy",
      EnterpriseStatusToString(device_cloud_policy->state()));
  return_value->SetString("device_id", device_data_store->device_id());
  return_value->SetString("device_token", device_data_store->device_token());
  return_value->SetString("gaia_token", device_data_store->gaia_token());
  return_value->SetString("machine_id", device_data_store->machine_id());
  return_value->SetString("machine_model", device_data_store->machine_model());
  return_value->SetString("user_name", device_data_store->user_name());
  return_value->SetBoolean("device_token_cache_loaded",
                           device_data_store->token_cache_loaded());
  return_value->SetBoolean("user_token_cache_loaded",
                           user_data_store->token_cache_loaded());
  // Get PolicyMaps.
  return_value->Set("device_mandatory_policies",
                    CreateDictionaryWithPolicies(device_cloud_policy,
                        policy::POLICY_LEVEL_MANDATORY));
  return_value->Set("user_mandatory_policies",
                    CreateDictionaryWithPolicies(user_cloud_policy,
                        policy::POLICY_LEVEL_MANDATORY));
  return_value->Set("device_recommended_policies",
      CreateDictionaryWithPolicies(device_cloud_policy,
          policy::POLICY_LEVEL_RECOMMENDED));
  return_value->Set("user_recommended_policies",
      CreateDictionaryWithPolicies(user_cloud_policy,
          policy::POLICY_LEVEL_RECOMMENDED));
  reply.SendSuccess(return_value.get());
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
  const UserManager* user_manager = UserManager::Get();
  if (!user_manager) {
    reply.SendError("No user manager!");
    return;
  }

  if (user_manager->IsUserLoggedIn()) {
    chromeos::accessibility::EnableSpokenFeedback(enabled, NULL);
  } else {
    ExistingUserController* controller =
        ExistingUserController::current_controller();
    chromeos::WebUILoginDisplayHost* webui_login_display_host =
        static_cast<chromeos::WebUILoginDisplayHost*>(
            controller->login_display_host());
    chromeos::accessibility::EnableSpokenFeedback(
        enabled, webui_login_display_host->GetOobeUI()->web_ui());
  }

  reply.SendSuccess(return_value.get());
}

void TestingAutomationProvider::IsSpokenFeedbackEnabled(
    DictionaryValue* args, IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  return_value->SetBoolean("spoken_feedback",
                           chromeos::accessibility::IsSpokenFeedbackEnabled());
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

void TestingAutomationProvider::GetUpdateInfo(DictionaryValue* args,
                                              IPC::Message* reply_message) {
  AutomationJSONReply* reply = new AutomationJSONReply(this, reply_message);
  DBusThreadManager::Get()->GetUpdateEngineClient()
      ->GetReleaseTrack(base::Bind(GetReleaseTrackCallback, reply));
}

void TestingAutomationProvider::UpdateCheck(
    DictionaryValue* args,
    IPC::Message* reply_message) {
  AutomationJSONReply* reply = new AutomationJSONReply(this, reply_message);
  DBusThreadManager::Get()->GetUpdateEngineClient()
      ->RequestUpdateCheck(base::Bind(UpdateCheckCallback, reply));
}

void TestingAutomationProvider::SetReleaseTrack(DictionaryValue* args,
                                                IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  std::string track;
  if (!args->GetString("track", &track)) {
    reply.SendError("Invalid or missing args.");
    return;
  }

  DBusThreadManager::Get()->GetUpdateEngineClient()->SetReleaseTrack(track);
  reply.SendSuccess(NULL);
}

void TestingAutomationProvider::GetVolumeInfo(DictionaryValue* args,
                                              IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  chromeos::AudioHandler* audio_handler = chromeos::AudioHandler::GetInstance();
  if (!audio_handler) {
    reply.SendError("AudioHandler not initialized.");
    return;
  }
  return_value->SetDouble("volume", audio_handler->GetVolumePercent());
  return_value->SetBoolean("is_mute", audio_handler->IsMuted());
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
  chromeos::AudioHandler* audio_handler = chromeos::AudioHandler::GetInstance();
  if (!audio_handler) {
    reply.SendError("AudioHandler not initialized.");
    return;
  }
  audio_handler->SetVolumePercent(volume_percent);
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
  chromeos::AudioHandler* audio_handler = chromeos::AudioHandler::GetInstance();
  if (!audio_handler) {
    reply.SendError("AudioHandler not initialized.");
    return;
  }
  audio_handler->SetMuted(mute);
  reply.SendSuccess(NULL);
}

void TestingAutomationProvider::OpenCrosh(DictionaryValue* args,
                                          IPC::Message* reply_message) {
  new NavigationNotificationObserver(
      NULL, this, reply_message, 1, false, true);
  ash::Shell::GetInstance()->delegate()->OpenCrosh();
}

void TestingAutomationProvider::CaptureProfilePhoto(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  chromeos::TakePhotoDialog* take_photo_dialog =
      new chromeos::TakePhotoDialog(NULL);

  // Set up an observer (it will delete itself).
  take_photo_dialog->AddObserver(new PhotoCaptureObserver(
      this, reply_message));

  views::Widget* window = views::Widget::CreateWindowWithParent(
      take_photo_dialog, browser->window()->GetNativeWindow());
  window->SetAlwaysOnTop(true);
  window->Show();
}

void TestingAutomationProvider::AddChromeosObservers() {
  power_manager_observer_ = new PowerManagerClientObserverForTesting;
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
      AddObserver(power_manager_observer_);
}

void TestingAutomationProvider::RemoveChromeosObservers() {
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
      RemoveObserver(power_manager_observer_);
  delete power_manager_observer_;
  power_manager_observer_ = NULL;
}
