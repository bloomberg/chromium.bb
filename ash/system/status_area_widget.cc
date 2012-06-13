// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/status_area_widget.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "ash/system/bluetooth/bluetooth_observer.h"
#include "ash/system/network/network_observer.h"
#include "ash/system/status_area_widget_delegate.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/web_notification/web_notification_tray.h"
#include "base/i18n/time_formatting.h"
#include "base/utf_string_conversions.h"
#include "ui/aura/window.h"

namespace ash {

namespace {

class DummySystemTrayDelegate : public SystemTrayDelegate {
 public:
  DummySystemTrayDelegate()
      : muted_(false),
        wifi_enabled_(true),
        cellular_enabled_(true),
        bluetooth_enabled_(true),
        volume_(0.5),
        caps_lock_enabled_(false) {
  }

  virtual ~DummySystemTrayDelegate() {}

 private:
  virtual bool GetTrayVisibilityOnStartup() OVERRIDE { return true; }

  // Overridden from SystemTrayDelegate:
  virtual const string16 GetUserDisplayName() const OVERRIDE {
    return UTF8ToUTF16("Über tray Über tray Über tray Über tray");
  }

  virtual const std::string GetUserEmail() const OVERRIDE {
    return "über@tray";
  }

  virtual const gfx::ImageSkia& GetUserImage() const OVERRIDE {
    return null_image_;
  }

  virtual user::LoginStatus GetUserLoginStatus() const OVERRIDE {
    return user::LOGGED_IN_USER;
  }

  virtual bool SystemShouldUpgrade() const OVERRIDE {
    return true;
  }

  virtual base::HourClockType GetHourClockType() const OVERRIDE {
    return base::k24HourClock;
  }

  virtual PowerSupplyStatus GetPowerSupplyStatus() const OVERRIDE {
    return PowerSupplyStatus();
  }

  virtual void RequestStatusUpdate() const OVERRIDE {
  }

  virtual void ShowSettings() OVERRIDE {
  }

  virtual void ShowDateSettings() OVERRIDE {
  }

  virtual void ShowNetworkSettings() OVERRIDE {
  }

  virtual void ShowBluetoothSettings() OVERRIDE {
  }

  virtual void ShowDriveSettings() OVERRIDE {
  }

  virtual void ShowIMESettings() OVERRIDE {
  }

  virtual void ShowHelp() OVERRIDE {
  }

  virtual bool IsAudioMuted() const OVERRIDE {
    return muted_;
  }

  virtual void SetAudioMuted(bool muted) OVERRIDE {
    muted_ = muted;
  }

  virtual float GetVolumeLevel() const OVERRIDE {
    return volume_;
  }

  virtual void SetVolumeLevel(float volume) OVERRIDE {
    volume_ = volume;
  }

  virtual bool IsCapsLockOn() const OVERRIDE {
    return caps_lock_enabled_;
  }

  virtual void SetCapsLockEnabled(bool enabled) OVERRIDE {
    caps_lock_enabled_ = enabled;
  }

  virtual bool IsInAccessibilityMode() const OVERRIDE {
    return false;
  }

  virtual void SetEnableSpokenFeedback(bool enable) OVERRIDE {}

  virtual void ShutDown() OVERRIDE {}

  virtual void SignOut() OVERRIDE {
    MessageLoop::current()->Quit();
  }

  virtual void RequestLockScreen() OVERRIDE {}

  virtual void RequestRestart() OVERRIDE {}

  virtual void GetAvailableBluetoothDevices(
      BluetoothDeviceList* list) OVERRIDE {
  }

  virtual void ToggleBluetoothConnection(const std::string& address) OVERRIDE {
  }

  virtual void GetCurrentIME(IMEInfo* info) OVERRIDE {
  }

  virtual void GetAvailableIMEList(IMEInfoList* list) OVERRIDE {
  }

  virtual void GetCurrentIMEProperties(IMEPropertyInfoList* list) OVERRIDE {
  }

  virtual void SwitchIME(const std::string& ime_id) OVERRIDE {
  }

  virtual void ActivateIMEProperty(const std::string& key) OVERRIDE {
  }

  virtual void CancelDriveOperation(const FilePath&) OVERRIDE {
  }

  virtual void GetDriveOperationStatusList(
      ash::DriveOperationStatusList*) OVERRIDE {
  }

  virtual void GetMostRelevantNetworkIcon(NetworkIconInfo* info,
                                          bool large) OVERRIDE {
  }

  virtual void GetAvailableNetworks(
      std::vector<NetworkIconInfo>* list) OVERRIDE {
  }

  virtual void ConnectToNetwork(const std::string& network_id) OVERRIDE {
  }

  virtual void GetNetworkAddresses(std::string* ip_address,
                                   std::string* ethernet_mac_address,
                                   std::string* wifi_mac_address) OVERRIDE {
    *ip_address = "127.0.0.1";
    *ethernet_mac_address = "00:11:22:33:44:55";
    *wifi_mac_address = "66:77:88:99:00:11";
  }

  virtual void RequestNetworkScan() OVERRIDE {
  }

  virtual void AddBluetoothDevice() OVERRIDE {
  }

  virtual void ToggleAirplaneMode() OVERRIDE {
  }

  virtual void ToggleWifi() OVERRIDE {
    wifi_enabled_ = !wifi_enabled_;
    ash::NetworkObserver* observer =
        ash::Shell::GetInstance()->system_tray()->network_observer();
    if (observer) {
      ash::NetworkIconInfo info;
      observer->OnNetworkRefresh(info);
    }
  }

  virtual void ToggleMobile() OVERRIDE {
    cellular_enabled_ = !cellular_enabled_;
    ash::NetworkObserver* observer =
        ash::Shell::GetInstance()->system_tray()->network_observer();
    if (observer) {
      ash::NetworkIconInfo info;
      observer->OnNetworkRefresh(info);
    }
  }

  virtual void ToggleBluetooth() OVERRIDE {
    bluetooth_enabled_ = !bluetooth_enabled_;
    ash::BluetoothObserver* observer =
        ash::Shell::GetInstance()->system_tray()->bluetooth_observer();
    if (observer)
      observer->OnBluetoothRefresh();
  }

  virtual void ShowOtherWifi() OVERRIDE {
  }

  virtual void ShowOtherCellular() OVERRIDE {
  }

  virtual bool IsNetworkConnected() OVERRIDE {
    return true;
  }

  virtual bool GetWifiAvailable() OVERRIDE {
    return true;
  }

  virtual bool GetMobileAvailable() OVERRIDE {
    return true;
  }

  virtual bool GetBluetoothAvailable() OVERRIDE {
    return true;
  }

  virtual bool GetWifiEnabled() OVERRIDE {
    return wifi_enabled_;
  }

  virtual bool GetMobileEnabled() OVERRIDE {
    return cellular_enabled_;
  }

  virtual bool GetBluetoothEnabled() OVERRIDE {
    return bluetooth_enabled_;
  }

  virtual bool GetMobileScanSupported() OVERRIDE {
    return true;
  }

  virtual bool GetCellularCarrierInfo(std::string* carrier_id,
                                      std::string* topup_url,
                                      std::string* setup_url) OVERRIDE {
    return false;
  }

  virtual void ShowCellularURL(const std::string& url) OVERRIDE {
  }

  virtual void ChangeProxySettings() OVERRIDE {
  }

  bool muted_;
  bool wifi_enabled_;
  bool cellular_enabled_;
  bool bluetooth_enabled_;
  float volume_;
  bool caps_lock_enabled_;
  gfx::ImageSkia null_image_;

  DISALLOW_COPY_AND_ASSIGN(DummySystemTrayDelegate);
};

}  // namespace

namespace internal {

StatusAreaWidget::StatusAreaWidget()
    : widget_delegate_(new internal::StatusAreaWidgetDelegate),
      system_tray_(NULL),
      web_notification_tray_(NULL) {
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.delegate = widget_delegate_;
  params.parent =
      Shell::GetPrimaryRootWindowController()->GetContainer(
          ash::internal::kShellWindowId_StatusContainer);
  params.transparent = true;
  Init(params);
  set_focus_on_creation(false);
  SetContentsView(widget_delegate_);
  GetNativeView()->SetName("StatusAreaWidget");
}

StatusAreaWidget::~StatusAreaWidget() {
}

void StatusAreaWidget::CreateTrayViews(ShellDelegate* shell_delegate) {
  AddWebNotificationTray(new WebNotificationTray(this));
  AddSystemTray(new SystemTray(), shell_delegate);
}

void StatusAreaWidget::Shutdown() {
  // Destroy the trays early, causing them to be removed from the view
  // hierarchy. Do not used scoped pointers since we don't want to destroy them
  // in the destructor if Shutdown() is not called (e.g. in tests).
  delete system_tray_;
  system_tray_ = NULL;
  delete web_notification_tray_;
  web_notification_tray_ = NULL;
}

void StatusAreaWidget::AddSystemTray(SystemTray* system_tray,
                                     ShellDelegate* shell_delegate) {
  system_tray_ = system_tray;
  widget_delegate_->AddTray(system_tray);
  system_tray_->Initialize();  // Called after added to widget.

  if (shell_delegate) {
    system_tray_delegate_.reset(
        shell_delegate->CreateSystemTrayDelegate(system_tray));
  }
  if (!system_tray_delegate_.get())
    system_tray_delegate_.reset(new DummySystemTrayDelegate());

  system_tray->CreateItems();  // Called after delegate is created.
}

void StatusAreaWidget::AddWebNotificationTray(
    WebNotificationTray* web_notification_tray) {
  web_notification_tray_ = web_notification_tray;
  widget_delegate_->AddTray(web_notification_tray);
}

void StatusAreaWidget::SetShelfAlignment(ShelfAlignment alignment) {
  widget_delegate_->set_alignment(alignment);
  widget_delegate_->UpdateLayout();
  if (system_tray_)
    system_tray_->SetShelfAlignment(alignment);
  if (web_notification_tray_)
    web_notification_tray_->SetShelfAlignment(alignment);
}

void StatusAreaWidget::SetPaintsBackground(
    bool value,
    internal::BackgroundAnimator::ChangeType change_type) {
  if (system_tray_)
    system_tray_->SetPaintsBackground(value, change_type);
  if (web_notification_tray_)
    web_notification_tray_->SetPaintsBackground(value, change_type);
}

void StatusAreaWidget::ShowWebNotificationBubble(UserAction user_action) {
  if (system_tray_ && system_tray_->IsBubbleVisible()) {
    // User actions should always hide the system tray bubble first.
    DCHECK(user_action != USER_ACTION);
    // Don't immediately show the web notification bubble if the system tray
    // bubble is visible.
    return;
  }
  DCHECK(web_notification_tray_);
  web_notification_tray_->ShowBubble();
  // Disable showing system notifications while viewing web notifications.
  if (system_tray_)
    system_tray_->SetHideNotifications(true);
}

void StatusAreaWidget::HideWebNotificationBubble() {
  DCHECK(web_notification_tray_);
  web_notification_tray_->HideBubble();
  // Show any hidden or suppressed system notifications.
  if (system_tray_)
    system_tray_->SetHideNotifications(false);
}

}  // namespace internal
}  // namespace ash
