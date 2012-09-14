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
#include "ash/volume_control_delegate.h"
#include "ash/wm/shelf_layout_manager.h"
#include "base/i18n/time_formatting.h"
#include "base/utf_string_conversions.h"
#include "ui/aura/window.h"
#include "ui/gfx/screen.h"

namespace ash {

namespace {

class DummyVolumeControlDelegate : public VolumeControlDelegate {
 public:
  DummyVolumeControlDelegate() {}
  virtual ~DummyVolumeControlDelegate() {}

  virtual bool HandleVolumeMute(const ui::Accelerator& accelerator) OVERRIDE {
    return true;
  }
  virtual bool HandleVolumeDown(const ui::Accelerator& accelerator) OVERRIDE {
    return true;
  }
  virtual bool HandleVolumeUp(const ui::Accelerator& accelerator) OVERRIDE {
    return true;
  }
  virtual void SetVolumePercent(double percent) OVERRIDE {
  }
  virtual bool IsAudioMuted() const OVERRIDE {
    return true;
  }
  virtual void SetAudioMuted(bool muted) OVERRIDE {
  }
  virtual float GetVolumeLevel() const OVERRIDE {
    return 0.0;
  }
  virtual void SetVolumeLevel(float level) OVERRIDE {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyVolumeControlDelegate);
};

class DummySystemTrayDelegate : public SystemTrayDelegate {
 public:
  DummySystemTrayDelegate()
      : wifi_enabled_(true),
        cellular_enabled_(true),
        bluetooth_enabled_(true),
        caps_lock_enabled_(false),
        volume_control_delegate_(
            ALLOW_THIS_IN_INITIALIZER_LIST(new DummyVolumeControlDelegate)) {
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

  virtual void ShowDisplaySettings() OVERRIDE {
  }

  virtual void ShowDriveSettings() OVERRIDE {
  }

  virtual void ShowIMESettings() OVERRIDE {
  }

  virtual void ShowHelp() OVERRIDE {
  }

  virtual void ShutDown() OVERRIDE {
    MessageLoop::current()->Quit();
  }

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

  virtual VolumeControlDelegate* GetVolumeControlDelegate() const OVERRIDE {
    return volume_control_delegate_.get();
  }

  virtual void SetVolumeControlDelegate(
      scoped_ptr<VolumeControlDelegate> delegate) OVERRIDE {
    volume_control_delegate_.swap(delegate);
  }


  bool wifi_enabled_;
  bool cellular_enabled_;
  bool bluetooth_enabled_;
  bool caps_lock_enabled_;
  gfx::ImageSkia null_image_;
  scoped_ptr<VolumeControlDelegate> volume_control_delegate_;

  DISALLOW_COPY_AND_ASSIGN(DummySystemTrayDelegate);
};

}  // namespace

namespace internal {

StatusAreaWidget::StatusAreaWidget()
    : status_area_widget_delegate_(new internal::StatusAreaWidgetDelegate),
      system_tray_(NULL),
      web_notification_tray_(NULL),
      login_status_(user::LOGGED_IN_NONE) {
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.delegate = status_area_widget_delegate_;
  params.parent =
      Shell::GetPrimaryRootWindowController()->GetContainer(
          ash::internal::kShellWindowId_StatusContainer);
  params.transparent = true;
  Init(params);
  set_focus_on_creation(false);
  SetContentsView(status_area_widget_delegate_);
  GetNativeView()->SetName("StatusAreaWidget");
}

StatusAreaWidget::~StatusAreaWidget() {
}

void StatusAreaWidget::CreateTrayViews(ShellDelegate* shell_delegate) {
  AddSystemTray(shell_delegate);
  AddWebNotificationTray();
  // Initialize() must be called after all trays have been created.
  if (system_tray_)
    system_tray_->Initialize();
  if (web_notification_tray_)
    web_notification_tray_->Initialize();
  UpdateAfterLoginStatusChange(system_tray_delegate_->GetUserLoginStatus());
}

void StatusAreaWidget::Shutdown() {
  // Destroy the trays early, causing them to be removed from the view
  // hierarchy. Do not used scoped pointers since we don't want to destroy them
  // in the destructor if Shutdown() is not called (e.g. in tests).
  system_tray_delegate_.reset();
  system_tray_ = NULL;
  delete system_tray_;
  web_notification_tray_ = NULL;
  delete web_notification_tray_;
}

bool StatusAreaWidget::ShouldShowLauncher() const {
  if ((system_tray_ && system_tray_->HasSystemBubble()) ||
      (web_notification_tray_ &&
       web_notification_tray_->IsMessageCenterBubbleVisible()))
    return true;

  if (!Shell::GetInstance()->shelf()->IsVisible())
    return false;

  // If the launcher is currently visible, don't hide the launcher if the mouse
  // is in any of the notification bubbles.
  return (system_tray_ && system_tray_->IsMouseInNotificationBubble()) ||
        (web_notification_tray_ &&
         web_notification_tray_->IsMouseInNotificationBubble());
}

void StatusAreaWidget::AddSystemTray(ShellDelegate* shell_delegate) {
  system_tray_ = new SystemTray(this);
  status_area_widget_delegate_->AddTray(system_tray_);

  if (shell_delegate) {
    system_tray_delegate_.reset(
        shell_delegate->CreateSystemTrayDelegate(system_tray_));
  }
  if (!system_tray_delegate_.get())
    system_tray_delegate_.reset(new DummySystemTrayDelegate());
}

void StatusAreaWidget::AddWebNotificationTray() {
  web_notification_tray_ = new WebNotificationTray(this);
  status_area_widget_delegate_->AddTray(web_notification_tray_);
}

void StatusAreaWidget::SetShelfAlignment(ShelfAlignment alignment) {
  status_area_widget_delegate_->set_alignment(alignment);
  if (system_tray_)
    system_tray_->SetShelfAlignment(alignment);
  if (web_notification_tray_)
    web_notification_tray_->SetShelfAlignment(alignment);
  status_area_widget_delegate_->UpdateLayout();
}

void StatusAreaWidget::SetPaintsBackground(
    bool value,
    internal::BackgroundAnimator::ChangeType change_type) {
  if (system_tray_)
    system_tray_->SetPaintsBackground(value, change_type);
  if (web_notification_tray_)
    web_notification_tray_->SetPaintsBackground(value, change_type);
}

void StatusAreaWidget::SetHideWebNotifications(bool hide) {
  if (web_notification_tray_)
    web_notification_tray_->SetHidePopupBubble(hide);
}

void StatusAreaWidget::SetHideSystemNotifications(bool hide) {
  if (system_tray_)
    system_tray_->SetHideNotifications(hide);
}

bool StatusAreaWidget::ShouldShowWebNotifications() {
  return !(system_tray_ && system_tray_->IsAnyBubbleVisible());
}

void StatusAreaWidget::UpdateAfterLoginStatusChange(
    user::LoginStatus login_status) {
  if (login_status_ == login_status)
    return;
  login_status_ = login_status;
  if (system_tray_)
    system_tray_->UpdateAfterLoginStatusChange(login_status);
  if (web_notification_tray_)
    web_notification_tray_->UpdateAfterLoginStatusChange(login_status);
}

}  // namespace internal
}  // namespace ash
