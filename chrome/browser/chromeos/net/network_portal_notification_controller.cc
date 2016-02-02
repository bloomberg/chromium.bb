// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_portal_notification_controller.h"

#include <stdint.h>

#include <vector>

#include "ash/shell.h"
#include "ash/system/system_notifier.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/mobile/mobile_activator.h"
#include "chrome/browser/chromeos/net/network_portal_web_dialog.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/consumer_management_service.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_type_pattern.h"
#include "components/captive_portal/captive_portal_detector.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "extensions/browser/api/networking_config/networking_config_service.h"
#include "extensions/browser/api/networking_config/networking_config_service_factory.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_types.h"
#include "ui/message_center/notifier_settings.h"
#include "ui/views/widget/widget.h"

using message_center::Notification;

namespace chromeos {

namespace {

bool IsPortalNotificationEnabled() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableNetworkPortalNotification);
}

void CloseNotification() {
  message_center::MessageCenter::Get()->RemoveNotification(
      NetworkPortalNotificationController::kNotificationId, false);
}

Profile* GetProfileForPrimaryUser() {
  const user_manager::User* primary_user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  if (!primary_user)
    return nullptr;
  return ProfileHelper::Get()->GetProfileByUser(primary_user);
}

// Note that NetworkingConfigService may change after login as the profile
// switches from the login to the user's profile.
extensions::NetworkingConfigService* GetNetworkingConfigService(
    Profile* profile) {
  if (!profile)
    return nullptr;
  return extensions::NetworkingConfigServiceFactory::GetForBrowserContext(
      profile);
}

const extensions::Extension* LookupExtensionForRawSsid(
    extensions::NetworkingConfigService* networking_config_service,
    const std::vector<uint8_t>& raw_ssid) {
  DCHECK(networking_config_service);
  Profile* profile = GetProfileForPrimaryUser();
  if (!profile || !networking_config_service)
    return nullptr;
  std::string extension_id;
  std::string hex_ssid = base::HexEncode(raw_ssid.data(), raw_ssid.size());
  extension_id =
      networking_config_service->LookupExtensionIdForHexSsid(hex_ssid);
  if (extension_id.empty())
    return nullptr;
  return extensions::ExtensionRegistry::Get(profile)
      ->GetExtensionById(extension_id, extensions::ExtensionRegistry::ENABLED);
}

class NetworkPortalNotificationControllerDelegate
    : public message_center::NotificationDelegate {
 public:
  explicit NetworkPortalNotificationControllerDelegate(
      const std::string& extension_id,
      const std::string& guid,
      base::WeakPtr<NetworkPortalNotificationController> controller)
      : extension_id_(extension_id),
        guid_(guid),
        clicked_(false),
        controller_(controller) {}

  // Overridden from message_center::NotificationDelegate:
  void Display() override;
  void Close(bool by_user) override;
  void Click() override;
  void ButtonClick(int button_click) override;

 private:
  ~NetworkPortalNotificationControllerDelegate() override {}

  // ID of the extension responsible for network configuration of the network
  // that this notification is generated for. Empty if none.
  std::string extension_id_;

  // GUID of the network this notification is generated for.
  std::string guid_;

  bool clicked_;

  base::WeakPtr<NetworkPortalNotificationController> controller_;

  DISALLOW_COPY_AND_ASSIGN(NetworkPortalNotificationControllerDelegate);
};

void NetworkPortalNotificationControllerDelegate::Display() {
  UMA_HISTOGRAM_ENUMERATION(
      NetworkPortalNotificationController::kNotificationMetric,
      NetworkPortalNotificationController::NOTIFICATION_METRIC_DISPLAYED,
      NetworkPortalNotificationController::NOTIFICATION_METRIC_COUNT);
}

void NetworkPortalNotificationControllerDelegate::Close(bool by_user) {
  if (clicked_)
    return;
  NetworkPortalNotificationController::UserActionMetric metric =
      by_user ? NetworkPortalNotificationController::USER_ACTION_METRIC_CLOSED
              : NetworkPortalNotificationController::USER_ACTION_METRIC_IGNORED;
  UMA_HISTOGRAM_ENUMERATION(
      NetworkPortalNotificationController::kUserActionMetric, metric,
      NetworkPortalNotificationController::USER_ACTION_METRIC_COUNT);
}

void NetworkPortalNotificationControllerDelegate::Click() {
  clicked_ = true;
  UMA_HISTOGRAM_ENUMERATION(
      NetworkPortalNotificationController::kUserActionMetric,
      NetworkPortalNotificationController::USER_ACTION_METRIC_CLICKED,
      NetworkPortalNotificationController::USER_ACTION_METRIC_COUNT);

  Profile* profile = ProfileManager::GetActiveUserProfile();

  const bool disable_bypass_proxy_switch_present =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kDisableCaptivePortalBypassProxy);
  const bool use_incognito_profile =
      disable_bypass_proxy_switch_present
          ? false
          : (profile &&
             profile->GetPrefs()->GetBoolean(
                 prefs::kCaptivePortalAuthenticationIgnoresProxy));

  if (use_incognito_profile) {
    if (controller_)
      controller_->ShowDialog();
  } else {
    if (!profile)
      return;
    chrome::ScopedTabbedBrowserDisplayer displayer(
        profile, chrome::HOST_DESKTOP_TYPE_ASH);
    GURL url(captive_portal::CaptivePortalDetector::kDefaultURL);
    chrome::ShowSingletonTab(displayer.browser(), url);
  }
  CloseNotification();
}

void NetworkPortalNotificationControllerDelegate::ButtonClick(
    int button_index) {
  if (button_index ==
      NetworkPortalNotificationController::kUseExtensionButtonIndex) {
    Profile* profile = GetProfileForPrimaryUser();
    // The user decided to notify the extension to authenticate to the captive
    // portal. Notify the NetworkingConfigService, which in turn will notify the
    // extension. OnExtensionFinsihedAuthentication will be called back if the
    // authentication succeeded.
    extensions::NetworkingConfigServiceFactory::GetForBrowserContext(profile)
        ->DispatchPortalDetectedEvent(
            extension_id_, guid_,
            base::Bind(&NetworkPortalNotificationController::
                           OnExtensionFinishedAuthentication,
                       controller_));
  } else if (button_index ==
             NetworkPortalNotificationController::kOpenPortalButtonIndex) {
    Click();
  }
}

gfx::Image& GetImageForNotification() {
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  gfx::Image& icon = bundle.GetImageNamed(IDR_PORTAL_DETECTION_ALERT);
  return icon;
}

}  // namespace

// static
const int NetworkPortalNotificationController::kUseExtensionButtonIndex = 0;

// static
const int NetworkPortalNotificationController::kOpenPortalButtonIndex = 1;

// static
const char NetworkPortalNotificationController::kNotificationId[] =
    "chrome://net/network_portal_detector";

// static
const char NetworkPortalNotificationController::kNotificationMetric[] =
    "CaptivePortal.Notification.Status";

// static
const char NetworkPortalNotificationController::kUserActionMetric[] =
    "CaptivePortal.Notification.UserAction";

NetworkPortalNotificationController::NetworkPortalNotificationController(
    NetworkPortalDetector* network_portal_detector)
    : network_portal_detector_(network_portal_detector), weak_factory_(this) {
  if (NetworkHandler::IsInitialized()) {  // NULL for unit tests.
    NetworkHandler::Get()->network_state_handler()->AddObserver(this,
                                                                FROM_HERE);
  }
  if (network_portal_detector_)
    network_portal_detector_->AddObserver(this);
}

NetworkPortalNotificationController::~NetworkPortalNotificationController() {
  if (network_portal_detector_)
    network_portal_detector_->RemoveObserver(this);
  if (NetworkHandler::IsInitialized()) {
    NetworkHandler::Get()->network_state_handler()->RemoveObserver(this,
                                                                   FROM_HERE);
  }
}

void NetworkPortalNotificationController::DefaultNetworkChanged(
    const NetworkState* network) {
  if (!network)
    return;
  Profile* profile = GetProfileForPrimaryUser();
  extensions::NetworkingConfigService* networking_config_service =
      GetNetworkingConfigService(profile);
  if (!networking_config_service)
    return;
  networking_config_service->ResetAuthenticationResult();
}

void NetworkPortalNotificationController::OnPortalDetectionCompleted(
    const NetworkState* network,
    const NetworkPortalDetector::CaptivePortalState& state) {
  if (!IsPortalNotificationEnabled())
    return;

  if (!network ||
      state.status != NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL) {
    last_network_path_.clear();

    // In browser tests we initiate fake network portal detection, but network
    // state usually stays connected. This way, after dialog is shown, it is
    // immediately closed. The testing check below prevents dialog from closing.
    if (dialog_ &&
        (!ignore_no_network_for_testing_ ||
         state.status == NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE)) {
      dialog_->Close();
    }

    CloseNotification();
    return;
  }

  // Don't do anything if we're currently activating the device.
  if (MobileActivator::GetInstance()->RunningActivation())
    return;

  // Don't do anything if notification for |network| already was
  // displayed.
  if (network->path() == last_network_path_)
    return;
  last_network_path_ = network->path();

  if (ash::Shell::HasInstance()) {
    ash::Shell::GetInstance()
        ->system_tray_notifier()
        ->NotifyOnCaptivePortalDetected(network->path());
  }

  message_center::MessageCenter::Get()->AddNotification(
      GetNotification(network, state));
}

void NetworkPortalNotificationController::ShowDialog() {
  if (dialog_)
    return;

  Profile* signin_profile = ProfileHelper::GetSigninProfile();
  dialog_ = new NetworkPortalWebDialog(weak_factory_.GetWeakPtr());
  dialog_->SetWidget(views::Widget::GetWidgetForNativeWindow(
      chrome::ShowWebDialog(nullptr, signin_profile, dialog_)));
}

void NetworkPortalNotificationController::OnDialogDestroyed(
    const NetworkPortalWebDialog* dialog) {
  if (dialog == dialog_) {
    dialog_ = nullptr;
    ProfileHelper::Get()->ClearSigninProfile(base::Closure());
  }
}

scoped_ptr<message_center::Notification>
NetworkPortalNotificationController::CreateDefaultCaptivePortalNotification(
    const NetworkState* network) {
  message_center::RichNotificationData data;
  scoped_refptr<NetworkPortalNotificationControllerDelegate> delegate(
      new NetworkPortalNotificationControllerDelegate(
          std::string(), network->guid(), weak_factory_.GetWeakPtr()));
  gfx::Image& icon = GetImageForNotification();
  message_center::NotifierId notifier_id(
      message_center::NotifierId::SYSTEM_COMPONENT,
      ash::system_notifier::kNotifierNetworkPortalDetector);
  base::string16 notificationText;
  bool is_wifi = NetworkTypePattern::WiFi().MatchesType(network->type());
  scoped_ptr<Notification> notification(new Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId,
      l10n_util::GetStringUTF16(
          is_wifi ? IDS_PORTAL_DETECTION_NOTIFICATION_TITLE_WIFI
                  : IDS_PORTAL_DETECTION_NOTIFICATION_TITLE_WIRED),
      l10n_util::GetStringFUTF16(
          is_wifi ? IDS_PORTAL_DETECTION_NOTIFICATION_MESSAGE_WIFI
                  : IDS_PORTAL_DETECTION_NOTIFICATION_MESSAGE_WIRED,
          base::UTF8ToUTF16(network->name())),
      icon, base::string16(), GURL(), notifier_id, data, delegate.get()));
  notification->SetSystemPriority();
  return notification;
}

scoped_ptr<message_center::Notification> NetworkPortalNotificationController::
    CreateCaptivePortalNotificationForExtension(
        const NetworkState* network,
        extensions::NetworkingConfigService* networking_config_service,
        const extensions::Extension* extension) {
  message_center::RichNotificationData data;
  scoped_refptr<NetworkPortalNotificationControllerDelegate> delegate(
      new NetworkPortalNotificationControllerDelegate(
          extension->id(), network->guid(), weak_factory_.GetWeakPtr()));
  gfx::Image& icon = GetImageForNotification();
  message_center::NotifierId notifier_id(
      message_center::NotifierId::SYSTEM_COMPONENT,
      ash::system_notifier::kNotifierNetworkPortalDetector);

  extensions::NetworkingConfigService::AuthenticationResult
      authentication_result =
          networking_config_service->GetAuthenticationResult();
  base::string16 notificationText;
  if (authentication_result.authentication_state ==
          extensions::NetworkingConfigService::NOTRY ||
      network->guid() != authentication_result.guid) {
    notificationText = l10n_util::GetStringFUTF16(
        IDS_PORTAL_DETECTION_NOTIFICATION_MESSAGE_ASK_WIFI,
        base::UTF8ToUTF16(network->name()));
    data.buttons.push_back(
        message_center::ButtonInfo(l10n_util::GetStringFUTF16(
            IDS_PORTAL_DETECTION_NOTIFICATION_BUTTON_EXTENSION,
            base::UTF8ToUTF16(extension->name()))));
    data.buttons.push_back(message_center::ButtonInfo(l10n_util::GetStringUTF16(
        IDS_PORTAL_DETECTION_NOTIFICATION_BUTTON_PORTAL)));
  } else {
    notificationText = l10n_util::GetStringFUTF16(
        IDS_PORTAL_DETECTION_NOTIFICATION_MESSAGE_FAILED_WIFI,
        base::UTF8ToUTF16(network->name()));
    data.buttons.push_back(
        message_center::ButtonInfo(l10n_util::GetStringFUTF16(
            IDS_PORTAL_DETECTION_NOTIFICATION_BUTTON_EXTENSION_RETRY,
            base::UTF8ToUTF16(extension->name()))));
    data.buttons.push_back(message_center::ButtonInfo(l10n_util::GetStringUTF16(
        IDS_PORTAL_DETECTION_NOTIFICATION_BUTTON_PORTAL)));
  }
  scoped_ptr<Notification> notification(new Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId,
      l10n_util::GetStringUTF16(IDS_PORTAL_DETECTION_NOTIFICATION_TITLE_WIFI),
      notificationText, icon, base::string16() /* display_source */, GURL(),
      notifier_id, data, delegate.get()));
  notification->SetSystemPriority();
  return notification;
}

scoped_ptr<Notification> NetworkPortalNotificationController::GetNotification(
    const NetworkState* network,
    const NetworkPortalDetector::CaptivePortalState& state) {
  base::string16 notificationText;
  Profile* profile = GetProfileForPrimaryUser();
  extensions::NetworkingConfigService* networking_config_service =
      GetNetworkingConfigService(profile);
  std::string extension_id;
  const extensions::Extension* extension = nullptr;
  if (networking_config_service) {
    extension = LookupExtensionForRawSsid(networking_config_service,
                                          network->raw_ssid());
  }
  if (extension) {
    return CreateCaptivePortalNotificationForExtension(
        network, networking_config_service, extension);
  } else {
    return CreateDefaultCaptivePortalNotification(network);
  }
}

void NetworkPortalNotificationController::OnExtensionFinishedAuthentication() {
  if (!retry_detection_callback_.is_null())
    retry_detection_callback_.Run();
}

void NetworkPortalNotificationController::SetIgnoreNoNetworkForTesting() {
  ignore_no_network_for_testing_ = true;
}

void NetworkPortalNotificationController::CloseDialog() {
  if (dialog_)
    dialog_->Close();
}

const NetworkPortalWebDialog*
NetworkPortalNotificationController::GetDialogForTesting() const {
  return dialog_;
}

}  // namespace chromeos
