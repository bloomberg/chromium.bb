// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/system_tray_client.h"

#include "ash/common/login_status.h"
#include "ash/common/wm_shell.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "ash/shell.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/accessibility_util.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/options/network_config_view.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/set_time_dialog.h"
#include "chrome/browser/chromeos/system/system_clock.h"
#include "chrome/browser/chromeos/ui/choose_mobile_network_dialog.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/ash/system_tray_delegate_chromeos.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/upgrade_detector.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/login/login_state.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/service_manager_connection.h"
#include "extensions/browser/api/vpn_provider/vpn_service.h"
#include "extensions/browser/api/vpn_provider/vpn_service_factory.h"
#include "net/base/escape.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/interfaces/window_manager.mojom.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

using chromeos::DBusThreadManager;
using chromeos::LoginState;
using views::Widget;

namespace {

SystemTrayClient* g_instance = nullptr;

void ShowSettingsSubPageForActiveUser(const std::string& sub_page) {
  chrome::ShowSettingsSubPageForProfile(ProfileManager::GetActiveUserProfile(),
                                        sub_page);
}

// Returns the severity of a pending Chrome / Chrome OS update.
ash::mojom::UpdateSeverity GetUpdateSeverity(UpgradeDetector* detector) {
  switch (detector->upgrade_notification_stage()) {
    case UpgradeDetector::UPGRADE_ANNOYANCE_NONE:
      return ash::mojom::UpdateSeverity::NONE;
    case UpgradeDetector::UPGRADE_ANNOYANCE_LOW:
      return ash::mojom::UpdateSeverity::LOW;
    case UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED:
      return ash::mojom::UpdateSeverity::ELEVATED;
    case UpgradeDetector::UPGRADE_ANNOYANCE_HIGH:
      return ash::mojom::UpdateSeverity::HIGH;
    case UpgradeDetector::UPGRADE_ANNOYANCE_SEVERE:
      return ash::mojom::UpdateSeverity::SEVERE;
    case UpgradeDetector::UPGRADE_ANNOYANCE_CRITICAL:
      return ash::mojom::UpdateSeverity::CRITICAL;
  }
  NOTREACHED();
  return ash::mojom::UpdateSeverity::CRITICAL;
}

}  // namespace

SystemTrayClient::SystemTrayClient() : binding_(this) {
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(ash::mojom::kServiceName, &system_tray_);
  // Register this object as the client interface implementation.
  system_tray_->SetClient(binding_.CreateInterfacePtrAndBind());

  // If this observes clock setting changes before ash comes up the IPCs will
  // be queued on |system_tray_|.
  g_browser_process->platform_part()->GetSystemClock()->AddObserver(this);

  registrar_.Add(this, chrome::NOTIFICATION_UPGRADE_RECOMMENDED,
                 content::NotificationService::AllSources());

  // If an upgrade is available at startup then tell ash about it.
  if (UpgradeDetector::GetInstance()->notify_upgrade())
    HandleUpdateAvailable();

  DCHECK(!g_instance);
  g_instance = this;
}

SystemTrayClient::~SystemTrayClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;

  g_browser_process->platform_part()->GetSystemClock()->RemoveObserver(this);
}

// static
SystemTrayClient* SystemTrayClient::Get() {
  return g_instance;
}

// static
ash::LoginStatus SystemTrayClient::GetUserLoginStatus() {
  if (!LoginState::Get()->IsUserLoggedIn())
    return ash::LoginStatus::NOT_LOGGED_IN;

  // Session manager client owns screen lock status.
  if (DBusThreadManager::Get()->GetSessionManagerClient()->IsScreenLocked())
    return ash::LoginStatus::LOCKED;

  LoginState::LoggedInUserType user_type =
      LoginState::Get()->GetLoggedInUserType();
  switch (user_type) {
    case LoginState::LOGGED_IN_USER_NONE:
      return ash::LoginStatus::NOT_LOGGED_IN;
    case LoginState::LOGGED_IN_USER_REGULAR:
      return ash::LoginStatus::USER;
    case LoginState::LOGGED_IN_USER_OWNER:
      return ash::LoginStatus::OWNER;
    case LoginState::LOGGED_IN_USER_GUEST:
      return ash::LoginStatus::GUEST;
    case LoginState::LOGGED_IN_USER_PUBLIC_ACCOUNT:
      return ash::LoginStatus::PUBLIC;
    case LoginState::LOGGED_IN_USER_SUPERVISED:
      return ash::LoginStatus::SUPERVISED;
    case LoginState::LOGGED_IN_USER_KIOSK_APP:
      return ash::LoginStatus::KIOSK_APP;
    case LoginState::LOGGED_IN_USER_ARC_KIOSK_APP:
      return ash::LoginStatus::ARC_KIOSK_APP;
  }
  NOTREACHED();
  return ash::LoginStatus::NOT_LOGGED_IN;
}

// static
int SystemTrayClient::GetDialogParentContainerId() {
  const ash::LoginStatus login_status = GetUserLoginStatus();
  if (login_status == ash::LoginStatus::NOT_LOGGED_IN ||
      login_status == ash::LoginStatus::LOCKED) {
    return ash::kShellWindowId_LockSystemModalContainer;
  }

  session_manager::SessionManager* const session_manager =
      session_manager::SessionManager::Get();
  const bool session_started = session_manager->IsSessionStarted();
  const bool is_in_secondary_login_screen =
      session_manager->IsInSecondaryLoginScreen();

  if (!session_started || is_in_secondary_login_screen)
    return ash::kShellWindowId_LockSystemModalContainer;

  return ash::kShellWindowId_SystemModalContainer;
}

// static
Widget* SystemTrayClient::CreateUnownedDialogWidget(
    views::WidgetDelegate* widget_delegate) {
  DCHECK(widget_delegate);
  Widget::InitParams params = views::DialogDelegate::GetDialogWidgetInitParams(
      widget_delegate, nullptr, nullptr, gfx::Rect());
  // Place the dialog in the appropriate modal dialog container, either above
  // or below the lock screen, based on the login state.
  int container_id = GetDialogParentContainerId();
  if (ash_util::IsRunningInMash()) {
    using ui::mojom::WindowManager;
    params.mus_properties[WindowManager::kContainerId_InitProperty] =
        mojo::ConvertTo<std::vector<uint8_t>>(container_id);
  } else {
    params.parent = ash::Shell::GetContainer(ash::Shell::GetPrimaryRootWindow(),
                                             container_id);
  }
  Widget* widget = new Widget;  // Owned by native widget.
  widget->Init(params);
  return widget;
}

void SystemTrayClient::SetFlashUpdateAvailable() {
  flash_update_available_ = true;
  HandleUpdateAvailable();
}

void SystemTrayClient::SetPrimaryTrayEnabled(bool enabled) {
  system_tray_->SetPrimaryTrayEnabled(enabled);
}

void SystemTrayClient::SetPrimaryTrayVisible(bool visible) {
  system_tray_->SetPrimaryTrayVisible(visible);
}

////////////////////////////////////////////////////////////////////////////////
// ash::mojom::SystemTrayClient:

void SystemTrayClient::ShowSettings() {
  ShowSettingsSubPageForActiveUser(std::string());
}

void SystemTrayClient::ShowDateSettings() {
  content::RecordAction(base::UserMetricsAction("ShowDateOptions"));
  // Everybody can change the time zone (even though it is a device setting).
  chrome::ShowSettingsSubPageForProfile(ProfileManager::GetActiveUserProfile(),
                                        chrome::kDateTimeSubPage);
}

void SystemTrayClient::ShowSetTimeDialog() {
  chromeos::SetTimeDialog::ShowDialogInContainer(GetDialogParentContainerId());
}

void SystemTrayClient::ShowDisplaySettings() {
  content::RecordAction(base::UserMetricsAction("ShowDisplayOptions"));
  ShowSettingsSubPageForActiveUser(chrome::kDisplaySubPage);
}

void SystemTrayClient::ShowPowerSettings() {
  content::RecordAction(base::UserMetricsAction("Tray_ShowPowerOptions"));
  ShowSettingsSubPageForActiveUser(chrome::kPowerSubPage);
}

void SystemTrayClient::ShowChromeSlow() {
  chrome::ScopedTabbedBrowserDisplayer displayer(
      ProfileManager::GetPrimaryUserProfile());
  chrome::ShowSlow(displayer.browser());
}

void SystemTrayClient::ShowIMESettings() {
  content::RecordAction(base::UserMetricsAction("OpenLanguageOptionsDialog"));
  ShowSettingsSubPageForActiveUser(chrome::kLanguageOptionsSubPage);
}

void SystemTrayClient::ShowHelp() {
  chrome::ShowHelpForProfile(ProfileManager::GetActiveUserProfile(),
                             chrome::HELP_SOURCE_MENU);
}

void SystemTrayClient::ShowAccessibilityHelp() {
  chrome::ScopedTabbedBrowserDisplayer displayer(
      ProfileManager::GetActiveUserProfile());
  chromeos::accessibility::ShowAccessibilityHelp(displayer.browser());
}

void SystemTrayClient::ShowAccessibilitySettings() {
  content::RecordAction(base::UserMetricsAction("ShowAccessibilitySettings"));
  ShowSettingsSubPageForActiveUser(chrome::kAccessibilitySubPage);
}

void SystemTrayClient::ShowPaletteHelp() {
  chrome::ScopedTabbedBrowserDisplayer displayer(
      ProfileManager::GetActiveUserProfile());
  chrome::ShowSingletonTab(displayer.browser(),
                           GURL(chrome::kChromePaletteHelpURL));
}

void SystemTrayClient::ShowPaletteSettings() {
  content::RecordAction(base::UserMetricsAction("ShowPaletteOptions"));
  ShowSettingsSubPageForActiveUser(chrome::kStylusSubPage);
}

void SystemTrayClient::ShowPublicAccountInfo() {
  chrome::ScopedTabbedBrowserDisplayer displayer(
      ProfileManager::GetActiveUserProfile());
  chrome::ShowPolicy(displayer.browser());
}

void SystemTrayClient::ShowNetworkConfigure(const std::string& network_id) {
  // UI is not available at the lock screen.
  if (session_manager::SessionManager::Get()->IsScreenLocked())
    return;

  // Dialog will default to the primary display.
  chromeos::NetworkConfigView::ShowForNetworkId(network_id,
                                                nullptr /* parent */);
}

void SystemTrayClient::ShowNetworkCreate(const std::string& type) {
  int container_id = GetDialogParentContainerId();
  if (type == shill::kTypeCellular) {
    chromeos::ChooseMobileNetworkDialog::ShowDialogInContainer(container_id);
    return;
  }
  chromeos::NetworkConfigView::ShowForType(type, nullptr /* parent */);
}

void SystemTrayClient::ShowThirdPartyVpnCreate(
    const std::string& extension_id) {
  const user_manager::User* primary_user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  if (!primary_user)
    return;

  Profile* profile =
      chromeos::ProfileHelper::Get()->GetProfileByUser(primary_user);
  if (!profile)
    return;

  // Request that the third-party VPN provider show its "add network" dialog.
  chromeos::VpnServiceFactory::GetForBrowserContext(profile)
      ->SendShowAddDialogToExtension(extension_id);
}

void SystemTrayClient::ShowNetworkSettings(const std::string& network_id) {
  if (!LoginState::Get()->IsUserLoggedIn() ||
      session_manager::SessionManager::Get()->IsInSecondaryLoginScreen()) {
    return;
  }

  std::string page = chrome::kInternetSubPage;
  if (!network_id.empty()) {
    if (base::FeatureList::IsEnabled(features::kMaterialDesignSettings))
      page = chrome::kNetworkDetailSubPage;
    page += "?guid=" + net::EscapeUrlEncodedData(network_id, true);
  }
  content::RecordAction(base::UserMetricsAction("OpenInternetOptionsDialog"));
  ShowSettingsSubPageForActiveUser(page);
}

void SystemTrayClient::ShowProxySettings() {
  LoginState* login_state = LoginState::Get();
  // User is not logged in.
  CHECK(!login_state->IsUserLoggedIn() ||
        login_state->GetLoggedInUserType() == LoginState::LOGGED_IN_USER_NONE);
  chromeos::LoginDisplayHost::default_host()->OpenProxySettings();
}

void SystemTrayClient::SignOut() {
  chrome::AttemptUserExit();
}

void SystemTrayClient::RequestRestartForUpdate() {
  // Flash updates on Chrome OS require device reboot.
  const chrome::RebootPolicy reboot_policy =
      flash_update_available_ ? chrome::RebootPolicy::kForceReboot
                              : chrome::RebootPolicy::kOptionalReboot;

  chrome::NotifyAndTerminate(true /* fast_path */, reboot_policy);
}

void SystemTrayClient::HandleUpdateAvailable() {
  // Show an update icon for Chrome updates and Flash component updates.
  UpgradeDetector* detector = UpgradeDetector::GetInstance();
  DCHECK(detector->notify_upgrade() || flash_update_available_);

  // Get the Chrome update severity.
  ash::mojom::UpdateSeverity severity = GetUpdateSeverity(detector);

  // Flash updates are low severity unless the Chrome severity is higher.
  if (flash_update_available_)
    severity = std::max(severity, ash::mojom::UpdateSeverity::LOW);

  system_tray_->ShowUpdateIcon(severity, detector->is_factory_reset_required());
}

////////////////////////////////////////////////////////////////////////////////
// chromeos::system::SystemClockObserver:

void SystemTrayClient::OnSystemClockChanged(
    chromeos::system::SystemClock* clock) {
  system_tray_->SetUse24HourClock(clock->ShouldUse24HourClock());
}

void SystemTrayClient::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_UPGRADE_RECOMMENDED, type);
  HandleUpdateAvailable();
}
