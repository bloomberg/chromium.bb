// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/system_tray_client.h"

#include "ash/common/login_status.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/wm_shell.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/accessibility/accessibility_util.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/options/network_config_view.h"
#include "chrome/browser/chromeos/set_time_dialog.h"
#include "chrome/browser/chromeos/system/system_clock.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/login/login_state.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/service_manager_connection.h"
#include "net/base/escape.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/base/l10n/l10n_util.h"

using chromeos::DBusThreadManager;
using chromeos::LoginState;

namespace {

const char kDisplaySettingsSubPageName[] = "display";
const char kPaletteSettingsSubPageName[] = "stylus-overlay";

SystemTrayClient* g_instance = nullptr;

void ShowSettingsSubPageForActiveUser(const std::string& sub_page) {
  chrome::ShowSettingsSubPageForProfile(ProfileManager::GetActiveUserProfile(),
                                        sub_page);
}

}  // namespace

SystemTrayClient::SystemTrayClient() {
  // If this observes clock setting changes before ash comes up the IPCs will
  // be queued on |system_tray_|.
  g_browser_process->platform_part()->GetSystemClock()->AddObserver(this);

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

  // TODO(mash): Need replacement for SessionStateDelegate. crbug.com/648964
  if (chrome::IsRunningInMash())
    return ash::kShellWindowId_SystemModalContainer;

  ash::WmShell* wm_shell = ash::WmShell::Get();
  const bool session_started =
      wm_shell->GetSessionStateDelegate()->IsActiveUserSessionStarted();
  const bool is_in_secondary_login_screen =
      wm_shell->GetSessionStateDelegate()->IsInSecondaryLoginScreen();

  if (!session_started || is_in_secondary_login_screen)
    return ash::kShellWindowId_LockSystemModalContainer;

  return ash::kShellWindowId_SystemModalContainer;
}

////////////////////////////////////////////////////////////////////////////////
// ash::mojom::SystemTrayClient:

void SystemTrayClient::ShowSettings() {
  ShowSettingsSubPageForActiveUser(std::string());
}

void SystemTrayClient::ShowDateSettings() {
  content::RecordAction(base::UserMetricsAction("ShowDateOptions"));
  std::string sub_page =
      std::string(chrome::kSearchSubPage) + "#" +
      l10n_util::GetStringUTF8(IDS_OPTIONS_SETTINGS_SECTION_TITLE_DATETIME);
  // Everybody can change the time zone (even though it is a device setting).
  chrome::ShowSettingsSubPageForProfile(ProfileManager::GetActiveUserProfile(),
                                        sub_page);
}

void SystemTrayClient::ShowSetTimeDialog() {
  chromeos::SetTimeDialog::ShowDialogInContainer(GetDialogParentContainerId());
}

void SystemTrayClient::ShowDisplaySettings() {
  content::RecordAction(base::UserMetricsAction("ShowDisplayOptions"));
  ShowSettingsSubPageForActiveUser(kDisplaySettingsSubPageName);
}

void SystemTrayClient::ShowPowerSettings() {
  content::RecordAction(base::UserMetricsAction("Tray_ShowPowerOptions"));
  ShowSettingsSubPageForActiveUser(chrome::kPowerOptionsSubPage);
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
  std::string sub_page = std::string(chrome::kSearchSubPage) + "#" +
                         l10n_util::GetStringUTF8(
                             IDS_OPTIONS_SETTINGS_SECTION_TITLE_ACCESSIBILITY);
  ShowSettingsSubPageForActiveUser(sub_page);
}

void SystemTrayClient::ShowPaletteHelp() {
  chrome::ScopedTabbedBrowserDisplayer displayer(
      ProfileManager::GetActiveUserProfile());
  chrome::ShowSingletonTab(displayer.browser(),
                           GURL(chrome::kChromePaletteHelpURL));
}

void SystemTrayClient::ShowPaletteSettings() {
  content::RecordAction(base::UserMetricsAction("ShowPaletteOptions"));
  ShowSettingsSubPageForActiveUser(kPaletteSettingsSubPageName);
}

void SystemTrayClient::ShowPublicAccountInfo() {
  chrome::ScopedTabbedBrowserDisplayer displayer(
      ProfileManager::GetActiveUserProfile());
  chrome::ShowPolicy(displayer.browser());
}

void SystemTrayClient::ShowNetworkConfigure(const std::string& network_id) {
  // UI is not available at the lock screen.
  // TODO(mash): Need replacement for SessionStateDelegate. crbug.com/648964
  if (!chrome::IsRunningInMash() &&
      ash::WmShell::Get()->GetSessionStateDelegate()->IsScreenLocked()) {
    return;
  }

  // Dialog will default to the primary display.
  chromeos::NetworkConfigView::ShowInContainer(network_id,
                                               GetDialogParentContainerId());
}

void SystemTrayClient::ShowNetworkSettings(const std::string& network_id) {
  if (!chrome::IsRunningInMash()) {
    // TODO(mash): Need replacement for SessionStateDelegate. crbug.com/648964
    if (!LoginState::Get()->IsUserLoggedIn() ||
        ash::WmShell::Get()
            ->GetSessionStateDelegate()
            ->IsInSecondaryLoginScreen())
      return;
  }

  std::string page = chrome::kInternetOptionsSubPage;
  if (!network_id.empty())
    page += "?guid=" + net::EscapeUrlEncodedData(network_id, true);
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

////////////////////////////////////////////////////////////////////////////////
// chromeos::system::SystemClockObserver:

void SystemTrayClient::OnSystemClockChanged(
    chromeos::system::SystemClock* clock) {
  ConnectToSystemTray();
  system_tray_->SetUse24HourClock(clock->ShouldUse24HourClock());
}

void SystemTrayClient::ConnectToSystemTray() {
  if (system_tray_.is_bound())
    return;

  service_manager::Connector* connector =
      content::ServiceManagerConnection::GetForProcess()->GetConnector();
  // Under mash the SystemTray interface is in the ash process. In classic ash
  // we provide it to ourself.
  if (chrome::IsRunningInMash())
    connector->ConnectToInterface("service:ash", &system_tray_);
  else
    connector->ConnectToInterface("service:content_browser", &system_tray_);

  // Tolerate ash crashing and coming back up.
  system_tray_.set_connection_error_handler(base::Bind(
      &SystemTrayClient::OnClientConnectionError, base::Unretained(this)));
}

void SystemTrayClient::OnClientConnectionError() {
  system_tray_.reset();
}
