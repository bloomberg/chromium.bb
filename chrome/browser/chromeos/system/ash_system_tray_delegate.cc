// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system/ash_system_tray_delegate.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/system/audio/audio_controller.h"
#include "ash/system/brightness/brightness_controller.h"
#include "ash/system/network/network_controller.h"
#include "ash/system/power/date_format_observer.h"
#include "ash/system/power/power_status_controller.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/user/update_controller.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/audio/audio_handler.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/dbus/dbus_thread_manager.h"
#include "chrome/browser/chromeos/dbus/power_manager_client.h"
#include "chrome/browser/chromeos/login/base_login_display_host.h"
#include "chrome/browser/chromeos/login/login_display_host.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/status/network_menu.h"
#include "chrome/browser/chromeos/status/network_menu_icon.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/upgrade_detector.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_service.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace {

ash::NetworkIconInfo CreateNetworkIconInfo(const Network* network,
                                           NetworkMenuIcon* network_icon) {
  ash::NetworkIconInfo info;
  info.name = UTF8ToUTF16(network->name());
  info.image = network_icon->GetBitmap(network, NetworkMenuIcon::SIZE_SMALL);
  info.unique_id = network->unique_id();
  return info;
}

class SystemTrayDelegate : public ash::SystemTrayDelegate,
                           public AudioHandler::VolumeObserver,
                           public PowerManagerClient::Observer,
                           public NetworkMenuIcon::Delegate,
                           public NetworkMenu::Delegate,
                           public NetworkLibrary::NetworkManagerObserver,
                           public NetworkLibrary::NetworkObserver,
                           public NetworkLibrary::CellularDataPlanObserver,
                           public content::NotificationObserver {
 public:
  explicit SystemTrayDelegate(ash::SystemTray* tray)
      : tray_(tray),
        network_icon_(ALLOW_THIS_IN_INITIALIZER_LIST(
                      new NetworkMenuIcon(this, NetworkMenuIcon::MENU_MODE))),
        network_icon_large_(ALLOW_THIS_IN_INITIALIZER_LIST(
                      new NetworkMenuIcon(this, NetworkMenuIcon::MENU_MODE))),
        network_menu_(ALLOW_THIS_IN_INITIALIZER_LIST(new NetworkMenu(this))) {
    AudioHandler::GetInstance()->AddVolumeObserver(this);
    DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(this);
    DBusThreadManager::Get()->GetPowerManagerClient()->RequestStatusUpdate(
        PowerManagerClient::UPDATE_USER);

    NetworkLibrary* crosnet = CrosLibrary::Get()->GetNetworkLibrary();
    crosnet->AddNetworkManagerObserver(this);
    OnNetworkManagerChanged(crosnet);
    crosnet->AddCellularDataPlanObserver(this);

    registrar_.Add(this,
                   chrome::NOTIFICATION_LOGIN_USER_CHANGED,
                   content::NotificationService::AllSources());
    registrar_.Add(this,
                   chrome::NOTIFICATION_UPGRADE_RECOMMENDED,
                   content::NotificationService::AllSources());

    InitializePrefChangeRegistrar();

    network_icon_large_->SetResourceSize(NetworkMenuIcon::SIZE_LARGE);
  }

  virtual ~SystemTrayDelegate() {
    AudioHandler* audiohandler = AudioHandler::GetInstance();
    if (audiohandler)
      audiohandler->RemoveVolumeObserver(this);
    DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(this);
  }

  // Overridden from ash::SystemTrayDelegate:
  virtual const std::string GetUserDisplayName() const OVERRIDE {
    return UserManager::Get()->GetLoggedInUser().GetDisplayName();
  }

  virtual const std::string GetUserEmail() const OVERRIDE {
    return UserManager::Get()->GetLoggedInUser().email();
  }

  virtual const SkBitmap& GetUserImage() const OVERRIDE {
    return UserManager::Get()->GetLoggedInUser().image();
  }

  virtual ash::user::LoginStatus GetUserLoginStatus() const OVERRIDE {
    UserManager* manager = UserManager::Get();
    if (!manager->IsUserLoggedIn())
      return ash::user::LOGGED_IN_NONE;
    if (manager->IsCurrentUserOwner())
      return ash::user::LOGGED_IN_OWNER;
    if (manager->IsLoggedInAsGuest())
      return ash::user::LOGGED_IN_GUEST;
    return ash::user::LOGGED_IN_USER;
  }

  virtual bool SystemShouldUpgrade() const OVERRIDE {
    return UpgradeDetector::GetInstance()->notify_upgrade();
  }

  virtual int GetSystemUpdateIconResource() const OVERRIDE {
    return UpgradeDetector::GetInstance()->GetIconResourceID(
        UpgradeDetector::UPGRADE_ICON_TYPE_MENU_ICON);
  }

  virtual base::HourClockType GetHourClockType() const OVERRIDE {
    Profile* profile = ProfileManager::GetDefaultProfile();
    return !profile || profile->GetPrefs()->GetBoolean(prefs::kUse24HourClock) ?
        base::k24HourClock : base::k12HourClock;
  }

  virtual void ShowSettings() OVERRIDE {
    GetAppropriateBrowser()->OpenOptionsDialog();
  }

  virtual void ShowDateSettings() OVERRIDE {
    GetAppropriateBrowser()->OpenAdvancedOptionsDialog();
  }

  virtual void ShowNetworkSettings() OVERRIDE {
    GetAppropriateBrowser()->OpenInternetOptionsDialog();
  }

  virtual void ShowHelp() OVERRIDE {
    GetAppropriateBrowser()->ShowHelpTab();
  }

  virtual bool IsAudioMuted() const OVERRIDE {
    return AudioHandler::GetInstance()->IsMuted();
  }

  virtual void SetAudioMuted(bool muted) OVERRIDE {
    return AudioHandler::GetInstance()->SetMuted(muted);
  }

  virtual float GetVolumeLevel() const OVERRIDE {
    return AudioHandler::GetInstance()->GetVolumePercent() / 100.f;
  }

  virtual void SetVolumeLevel(float level) OVERRIDE {
    AudioHandler::GetInstance()->SetVolumePercent(level * 100.f);
  }

  virtual void ShutDown() OVERRIDE {
    DBusThreadManager::Get()->GetPowerManagerClient()->RequestShutdown();
  }

  virtual void SignOut() OVERRIDE {
    BrowserList::AttemptUserExit();
  }

  virtual void RequestLockScreen() OVERRIDE {
    DBusThreadManager::Get()->GetPowerManagerClient()->
        NotifyScreenLockRequested();
  }

  virtual ash::NetworkIconInfo GetMostRelevantNetworkIcon(bool large) OVERRIDE {
    ash::NetworkIconInfo info;
    info.image = !large ? network_icon_->GetIconAndText(&info.description) :
        network_icon_large_->GetIconAndText(&info.description);
    return info;
  }

  virtual void GetAvailableNetworks(
      std::vector<ash::NetworkIconInfo>* list) OVERRIDE {
    NetworkLibrary* crosnet = CrosLibrary::Get()->GetNetworkLibrary();

    // Ethernet.
    if (crosnet->ethernet_available() && crosnet->ethernet_enabled()) {
      const EthernetNetwork* ethernet_network = crosnet->ethernet_network();
      if (ethernet_network) {
        ash::NetworkIconInfo info;
        info.image = network_icon_->GetBitmap(ethernet_network,
                                              NetworkMenuIcon::SIZE_SMALL);
        if (!ethernet_network->name().empty())
          info.name = UTF8ToUTF16(ethernet_network->name());
        else
          info.name =
              l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET);
        info.unique_id = ethernet_network->unique_id();
        list->push_back(info);
      }
    }

    // Wifi.
    if (crosnet->wifi_available() && crosnet->wifi_enabled()) {
      const WifiNetworkVector& wifi = crosnet->wifi_networks();
      for (size_t i = 0; i < wifi.size(); ++i)
        list->push_back(CreateNetworkIconInfo(wifi[i], network_icon_.get()));
    }

    // Cellular.
    if (crosnet->cellular_available() && crosnet->cellular_enabled()) {
      // TODO(sad): There are different cases for cellular networks, e.g.
      // de-activated networks, active networks that support data plan info,
      // networks with top-up URLs etc. All of these need to be handled
      // properly.
      const CellularNetworkVector& cell = crosnet->cellular_networks();
      for (size_t i = 0; i < cell.size(); ++i)
        list->push_back(CreateNetworkIconInfo(cell[i], network_icon_.get()));
    }

    // VPN (only if logged in).
    if (GetUserLoginStatus() == ash::user::LOGGED_IN_NONE)
      return;
    if (crosnet->connected_network() || crosnet->virtual_network_connected()) {
      const VirtualNetworkVector& vpns = crosnet->virtual_networks();
      for (size_t i = 0; i < vpns.size(); ++i)
        list->push_back(CreateNetworkIconInfo(vpns[i], network_icon_.get()));
    }
  }

  virtual void ConnectToNetwork(const std::string& network_id) OVERRIDE {
    NetworkLibrary* crosnet = CrosLibrary::Get()->GetNetworkLibrary();
    Network* network = crosnet->FindNetworkByUniqueId(network_id);
    if (network)
      network_menu_->ConnectToNetwork(network);
  }

  virtual void ToggleAirplaneMode() OVERRIDE {
    NetworkLibrary* crosnet = CrosLibrary::Get()->GetNetworkLibrary();
    crosnet->EnableOfflineMode(!crosnet->offline_mode());
  }

  virtual void ChangeProxySettings() OVERRIDE {
    CHECK(GetUserLoginStatus() == ash::user::LOGGED_IN_NONE);
    BaseLoginDisplayHost::default_host()->OpenProxySettings();
  }

 private:
  // Returns the last active browser. If there is no such browser, creates a new
  // browser window with an empty tab and returns it.
  Browser* GetAppropriateBrowser() {
    Browser* browser = BrowserList::GetLastActive();
    if (!browser)
      browser = Browser::NewEmptyWindow(ProfileManager::GetDefaultProfile());
    return browser;
  }

  void InitializePrefChangeRegistrar() {
    Profile* profile = ProfileManager::GetDefaultProfile();
    pref_registrar_.reset(new PrefChangeRegistrar);
    pref_registrar_->Init(profile->GetPrefs());
    pref_registrar_->Add(prefs::kUse24HourClock, this);
  }

  void NotifyRefreshNetwork() {
    ash::NetworkController* controller =
        ash::Shell::GetInstance()->network_controller();
    if (controller) {
      ash::NetworkIconInfo info;
      info.image = network_icon_->GetIconAndText(&info.description);
      controller->OnNetworkRefresh(info);
    }
  }

  void RefreshNetworkObserver(NetworkLibrary* crosnet) {
    const Network* network = crosnet->active_network();
    std::string new_path = network ? network->service_path() : std::string();
    if (active_network_path_ != new_path) {
      if (!active_network_path_.empty())
        crosnet->RemoveNetworkObserver(active_network_path_, this);
      if (!new_path.empty())
        crosnet->AddNetworkObserver(new_path, this);
      active_network_path_ = new_path;
    }
  }

  void RefreshNetworkDeviceObserver(NetworkLibrary* crosnet) {
    const NetworkDevice* cellular = crosnet->FindCellularDevice();
    std::string new_cellular_device_path = cellular ?
        cellular->device_path() : std::string();
    if (cellular_device_path_ != new_cellular_device_path)
      cellular_device_path_ = new_cellular_device_path;
  }

  // Overridden from AudioHandler::VolumeObserver.
  virtual void OnVolumeChanged() OVERRIDE {
    float level = AudioHandler::GetInstance()->GetVolumePercent() / 100.f;
    ash::Shell::GetInstance()->audio_controller()->
        OnVolumeChanged(level);
  }

  // Overridden from PowerManagerClient::Observer.
  virtual void BrightnessChanged(int level, bool user_initiated) OVERRIDE {
    ash::Shell::GetInstance()->brightness_controller()->
        OnBrightnessChanged(level / 100.f, user_initiated);
  }

  virtual void PowerChanged(const PowerSupplyStatus& power_status) OVERRIDE {
    ash::Shell::GetInstance()->power_status_controller()->
        OnPowerStatusChanged(power_status);
  }

  virtual void LockScreen() OVERRIDE {
  }

  virtual void UnlockScreen() OVERRIDE {
  }

  virtual void UnlockScreenFailed() OVERRIDE {
  }

  // TODO(sad): Override more from PowerManagerClient::Observer here (e.g.
  // PowerButtonStateChanged etc.).

  // Overridden from NetworkMenuIcon::Delegate.
  virtual void NetworkMenuIconChanged() OVERRIDE {
    NotifyRefreshNetwork();
  }

  // Overridden from NetworkMenu::Delegate.
  virtual views::MenuButton* GetMenuButton() OVERRIDE {
    return NULL;
  }

  virtual gfx::NativeWindow GetNativeWindow() const OVERRIDE {
    return ash::Shell::GetInstance()->GetContainer(
        GetUserLoginStatus() == ash::user::LOGGED_IN_NONE ?
            ash::internal::kShellWindowId_LockSystemModalContainer :
            ash::internal::kShellWindowId_SystemModalContainer);
  }

  virtual void OpenButtonOptions() OVERRIDE {
  }

  virtual bool ShouldOpenButtonOptions() const OVERRIDE {
    return false;
  }

  // Overridden from NetworkLibrary::NetworkManagerObserver.
  virtual void OnNetworkManagerChanged(NetworkLibrary* crosnet) OVERRIDE {
    RefreshNetworkObserver(crosnet);
    RefreshNetworkDeviceObserver(crosnet);

    // TODO: ShowOptionalMobileDataPromoNotification?

    NotifyRefreshNetwork();
  }

  // Overridden from NetworkLibrary::NetworkObserver.
  virtual void OnNetworkChanged(NetworkLibrary* crosnet,
      const Network* network) OVERRIDE {
    NotifyRefreshNetwork();
  }

  // Overridden from NetworkLibrary::CellularDataPlanObserver.
  virtual void OnCellularDataPlanChanged(NetworkLibrary* crosnet) OVERRIDE {
    NotifyRefreshNetwork();
  }

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    switch (type) {
      case chrome::NOTIFICATION_LOGIN_USER_CHANGED: {
        // Profile may have changed after login. So re-initialize the
        // pref-change registrar.
        InitializePrefChangeRegistrar();
        tray_->UpdateAfterLoginStatusChange(GetUserLoginStatus());
        break;
      }
      case chrome::NOTIFICATION_UPGRADE_RECOMMENDED: {
        ash::UpdateController* controller =
            ash::Shell::GetInstance()->update_controller();
        if (controller)
          controller->OnUpdateRecommended();
        break;
      }
      case chrome::NOTIFICATION_PREF_CHANGED: {
        DCHECK_EQ(*content::Details<std::string>(details).ptr(),
                  prefs::kUse24HourClock);
        ash::DateFormatObserver* observer =
            ash::Shell::GetInstance()->date_format_observer();
        if (observer)
          observer->OnDateFormatChanged();
        break;
      }
      default:
        NOTREACHED();
    }
  }

  ash::SystemTray* tray_;
  scoped_ptr<NetworkMenuIcon> network_icon_;
  scoped_ptr<NetworkMenuIcon> network_icon_large_;
  scoped_ptr<NetworkMenu> network_menu_;
  content::NotificationRegistrar registrar_;
  scoped_ptr<PrefChangeRegistrar> pref_registrar_;
  std::string cellular_device_path_;
  std::string active_network_path_;
  scoped_ptr<LoginHtmlDialog> proxy_settings_dialog_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayDelegate);
};

}  // namespace

ash::SystemTrayDelegate* CreateSystemTrayDelegate(ash::SystemTray* tray) {
  return new chromeos::SystemTrayDelegate(tray);
}

}  // namespace chromeos
