// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/tray_bluetooth.h"

#include "ash/shell.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_details_view.h"
#include "ash/system/tray/tray_item_more.h"
#include "ash/system/tray/tray_views.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace {
const int kDeviceListHeight = 276;
}

namespace ash {
namespace internal {

namespace tray {

class BluetoothDefaultView : public TrayItemMore {
 public:
  BluetoothDefaultView(SystemTrayItem* owner, bool show_more)
      : TrayItemMore(owner, show_more) {
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    SetImage(bundle.GetImageNamed(IDR_AURA_UBER_TRAY_BLUETOOTH).ToImageSkia());
    UpdateLabel();
  }

  virtual ~BluetoothDefaultView() {}

  void UpdateLabel() {
    ash::SystemTrayDelegate* delegate =
        ash::Shell::GetInstance()->system_tray_delegate();
    if (delegate->GetBluetoothAvailable()) {
      ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
      const string16 label =
          rb.GetLocalizedString(delegate->GetBluetoothEnabled() ?
              IDS_ASH_STATUS_TRAY_BLUETOOTH_ENABLED :
              IDS_ASH_STATUS_TRAY_BLUETOOTH_DISABLED);
      SetLabel(label);
      SetAccessibleName(label);
      SetVisible(true);
    } else {
      SetVisible(false);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothDefaultView);
};

class BluetoothDetailedView : public TrayDetailsView,
                              public ViewClickListener,
                              public views::ButtonListener {
 public:
  BluetoothDetailedView(SystemTrayItem* owner, user::LoginStatus login)
      : TrayDetailsView(owner),
        login_(login),
        add_device_(NULL),
        toggle_bluetooth_(NULL),
        enable_bluetooth_(NULL),
        bluetooth_discovering_(false) {
    CreateItems();
    Update();
  }

  virtual ~BluetoothDetailedView() {
    // Stop discovering bluetooth devices when exiting BT detailed view.
    BluetoothStopDiscovering();
  }

  void Update() {
    BluetoothStartDiscovering();
    UpdateBlueToothDeviceList();

    // Update UI.
    UpdateDeviceScrollList();
    UpdateHeaderEntry();
    Layout();
  }

 private:
  void CreateItems() {
    CreateScrollableList();
    AppendSettingsEntries();
    AppendHeaderEntry();
  }

  void BluetoothStartDiscovering() {
    ash::SystemTrayDelegate* delegate =
        ash::Shell::GetInstance()->system_tray_delegate();
    bool bluetooth_enabled = delegate->GetBluetoothEnabled();
    if (!bluetooth_discovering_ && bluetooth_enabled) {
      delegate->BluetoothStartDiscovering();
      throbber_->Start();
    } else if(!bluetooth_enabled) {
      throbber_->Stop();
    }
    bluetooth_discovering_ = bluetooth_enabled;
  }

  void BluetoothStopDiscovering() {
    ash::SystemTrayDelegate* delegate =
        ash::Shell::GetInstance()->system_tray_delegate();
    if (delegate && bluetooth_discovering_) {
      bluetooth_discovering_ = false;
      delegate->BluetoothStopDiscovering();
      throbber_->Stop();
    }
  }

  void UpdateBlueToothDeviceList() {
    connected_devices_.clear();
    paired_not_connected_devices_.clear();
    discovered_not_paired_devices_.clear();
    BluetoothDeviceList list;
    Shell::GetInstance()->system_tray_delegate()->
        GetAvailableBluetoothDevices(&list);
    for (size_t i = 0; i < list.size(); ++i) {
      if (list[i].connected)
        connected_devices_.push_back(list[i]);
      else if (list[i].paired)
        paired_not_connected_devices_.push_back(list[i]);
      else if (list[i].visible)
        discovered_not_paired_devices_.push_back(list[i]);
    }
  }

  void AppendHeaderEntry() {
    CreateSpecialRow(IDS_ASH_STATUS_TRAY_BLUETOOTH, this);

    if (login_ == user::LOGGED_IN_LOCKED)
      return;

    throbber_ = new ThrobberView;
    throbber_->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BLUETOOTH_DISCOVERING));
    footer()->AddThrobber(throbber_);

    // Do not allow toggling bluetooth in the lock screen.
    ash::SystemTrayDelegate* delegate =
        ash::Shell::GetInstance()->system_tray_delegate();
    toggle_bluetooth_ = new TrayPopupHeaderButton(this,
        IDR_AURA_UBER_TRAY_BLUETOOTH_ENABLED,
        IDR_AURA_UBER_TRAY_BLUETOOTH_DISABLED,
        IDR_AURA_UBER_TRAY_BLUETOOTH_ENABLED_HOVER,
        IDR_AURA_UBER_TRAY_BLUETOOTH_DISABLED_HOVER,
        IDS_ASH_STATUS_TRAY_BLUETOOTH);
    toggle_bluetooth_->SetToggled(!delegate->GetBluetoothEnabled());
    toggle_bluetooth_->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_DISABLE_BLUETOOTH));
    toggle_bluetooth_->SetToggledTooltipText(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ENABLE_BLUETOOTH));
    footer()->AddButton(toggle_bluetooth_);
 }

  void UpdateHeaderEntry() {
    if (toggle_bluetooth_) {
      toggle_bluetooth_->SetToggled(
          !ash::Shell::GetInstance()->system_tray_delegate()->
              GetBluetoothEnabled());
    }
  }

  void UpdateDeviceScrollList() {
    device_map_.clear();
    scroll_content()->RemoveAllChildViews(true);
    enable_bluetooth_ = NULL;

    ash::SystemTrayDelegate* delegate =
        ash::Shell::GetInstance()->system_tray_delegate();
    bool bluetooth_enabled = delegate->GetBluetoothEnabled();
    bool blueooth_available = delegate->GetBluetoothAvailable();
    if (blueooth_available && !bluetooth_enabled &&
        toggle_bluetooth_) {
      enable_bluetooth_ =
          AddScrollListItem(
              l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ENABLE_BLUETOOTH),
              gfx::Font::NORMAL, false, true);
    }

    AppendSameTypeDevicesToScrollList(
        connected_devices_, true, true, bluetooth_enabled);
    AppendSameTypeDevicesToScrollList(
        paired_not_connected_devices_, false, false, bluetooth_enabled);
    if (discovered_not_paired_devices_.size() > 0)
      AddScrollSeparator();
    AppendSameTypeDevicesToScrollList(
        discovered_not_paired_devices_, false, false, bluetooth_enabled);

    // Show user Bluetooth state if there is no bluetooth devices in list.
    if (device_map_.size() == 0) {
      if (blueooth_available && bluetooth_enabled) {
        AddScrollListItem(
            l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_BLUETOOTH_DISCOVERING),
            gfx::Font::NORMAL, false, true);
      }
    }

    scroll_content()->SizeToPreferredSize();
    static_cast<views::View*>(scroller())->Layout();
  }

  void AppendSameTypeDevicesToScrollList(const BluetoothDeviceList& list,
                                         bool bold,
                                         bool checked,
                                         bool enabled) {
    for (size_t i = 0; i < list.size(); ++i) {
      HoverHighlightView* container = AddScrollListItem(
          list[i].display_name,
          bold? gfx::Font::BOLD : gfx::Font::NORMAL,
          checked, enabled);
      device_map_[container] = list[i].address;
    }
  }

  HoverHighlightView* AddScrollListItem(const string16& text,
                                        gfx::Font::FontStyle style,
                                        bool checked,
                                        bool enabled) {
    HoverHighlightView* container = new HoverHighlightView(this);
    views::Label* label = container->AddCheckableLabel(text, style, checked);
    label->SetEnabled(enabled);
    scroll_content()->AddChildView(container);
    return container;
  }

  // Add settings entries.
  void AppendSettingsEntries() {
    // Add bluetooth device requires a browser window, hide it for non logged in
    // user.
    if (login_ == user::LOGGED_IN_NONE ||
        login_ == user::LOGGED_IN_LOCKED)
      return;

    ash::SystemTrayDelegate* delegate =
        ash::Shell::GetInstance()->system_tray_delegate();
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    HoverHighlightView* container = new HoverHighlightView(this);
    container->AddLabel(
        rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_BLUETOOTH_ADD_DEVICE),
        gfx::Font::NORMAL);
    container->SetEnabled(delegate->GetBluetoothAvailable());
    AddChildView(container);
    add_device_ = container;
  }

  // Returns true if the device with |device_id| is found in |device_list|,
  // and the display_name of the device will be returned in |display_name|.
  bool FoundDevice(const std::string& device_id,
                   const BluetoothDeviceList& device_list,
                   string16* display_name) {
    for (size_t i = 0; i < device_list.size(); ++i) {
      if (device_list[i].address == device_id) {
        *display_name = device_list[i].display_name;
        return true;
      }
    }
    return false;
  }

  // Updates UI of the clicked bluetooth device to show it is being connected
  // or disconnected if such an operation is going to be performed underway.
  void UpdateClickedDevice(std::string device_id, views::View* item_container) {
    string16 display_name;
    if (FoundDevice(device_id, connected_devices_, &display_name)) {
      display_name = l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_BLUETOOTH_DISCONNECTING, display_name);
    } else if (FoundDevice(device_id, paired_not_connected_devices_,
                           &display_name)) {
      display_name = l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_BLUETOOTH_CONNECTING, display_name);
    }
    if (display_name.length() > 0) {
      item_container->RemoveAllChildViews(true);
      static_cast<HoverHighlightView*>(item_container)->
          AddCheckableLabel(display_name, gfx::Font::BOLD, false);
      scroll_content()->SizeToPreferredSize();
      static_cast<views::View*>(scroller())->Layout();
    }
  }

  // Overridden from ViewClickListener.
  virtual void ClickedOn(views::View* sender) OVERRIDE {
    ash::SystemTrayDelegate* delegate =
        ash::Shell::GetInstance()->system_tray_delegate();
    if (sender == footer()->content()) {
      owner()->system_tray()->ShowDefaultView(BUBBLE_USE_EXISTING);
    } else if (sender == add_device_) {
      if (!delegate->GetBluetoothEnabled())
        delegate->ToggleBluetooth();
      delegate->AddBluetoothDevice();
    } else if (sender == enable_bluetooth_) {
      delegate->ToggleBluetooth();
    } else {
      std::map<views::View*, std::string>::iterator find;
      find = device_map_.find(sender);
      if (find != device_map_.end()) {
        std::string device_id = find->second;
        UpdateClickedDevice(device_id, sender);
        delegate->ToggleBluetoothConnection(device_id);
      }
    }
  }

  // Overridden from ButtonListener.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE {
    ash::SystemTrayDelegate* delegate =
        ash::Shell::GetInstance()->system_tray_delegate();
    if (sender == toggle_bluetooth_)
      delegate->ToggleBluetooth();
    else
      NOTREACHED();
  }

  user::LoginStatus login_;

  std::map<views::View*, std::string> device_map_;
  views::View* add_device_;
  ThrobberView* throbber_;
  TrayPopupHeaderButton* toggle_bluetooth_;
  HoverHighlightView* enable_bluetooth_;
  BluetoothDeviceList connected_devices_;
  BluetoothDeviceList paired_not_connected_devices_;
  BluetoothDeviceList discovered_not_paired_devices_;
  bool bluetooth_discovering_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothDetailedView);
};

}  // namespace tray

TrayBluetooth::TrayBluetooth(SystemTray* system_tray)
    : SystemTrayItem(system_tray),
      default_(NULL),
      detailed_(NULL) {
  Shell::GetInstance()->system_tray_notifier()->AddBluetoothObserver(this);
}

TrayBluetooth::~TrayBluetooth() {
  Shell::GetInstance()->system_tray_notifier()->RemoveBluetoothObserver(this);
}

views::View* TrayBluetooth::CreateTrayView(user::LoginStatus status) {
  return NULL;
}

views::View* TrayBluetooth::CreateDefaultView(user::LoginStatus status) {
  CHECK(default_ == NULL);
  default_ = new tray::BluetoothDefaultView(
      this, status != user::LOGGED_IN_LOCKED);
  return default_;
}

views::View* TrayBluetooth::CreateDetailedView(user::LoginStatus status) {
  if (!Shell::GetInstance()->system_tray_delegate()->GetBluetoothAvailable())
    return NULL;
  CHECK(detailed_ == NULL);
  detailed_ = new tray::BluetoothDetailedView(this, status);
  return detailed_;
}

void TrayBluetooth::DestroyTrayView() {
}

void TrayBluetooth::DestroyDefaultView() {
  default_ = NULL;
}

void TrayBluetooth::DestroyDetailedView() {
  detailed_ = NULL;
}

void TrayBluetooth::UpdateAfterLoginStatusChange(user::LoginStatus status) {
}

void TrayBluetooth::OnBluetoothRefresh() {
  if (default_)
    default_->UpdateLabel();
  else if (detailed_)
    detailed_->Update();
}

void TrayBluetooth::OnBluetoothDiscoveringChanged() {
  if (!detailed_)
    return;
  detailed_->Update();
}

}  // namespace internal
}  // namespace ash
