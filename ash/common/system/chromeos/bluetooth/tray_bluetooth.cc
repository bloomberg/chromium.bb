// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/bluetooth/tray_bluetooth.h"

#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/system/tray/fixed_sized_scroll_view.h"
#include "ash/common/system/tray/hover_highlight_view.h"
#include "ash/common/system/tray/system_tray.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/system/tray/system_tray_notifier.h"
#include "ash/common/system/tray/throbber_view.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_details_view.h"
#include "ash/common/system/tray/tray_item_more.h"
#include "ash/common/system/tray/tray_popup_header_button.h"
#include "ash/common/system/tray/tray_popup_item_style.h"
#include "ash/common/wm_shell.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {
namespace tray {
namespace {

// Updates bluetooth device |device| in the |list|. If it is new, append to the
// end of the |list|; otherwise, keep it at the same place, but update the data
// with new device info provided by |device|.
void UpdateBluetoothDeviceListHelper(BluetoothDeviceList* list,
                                     const BluetoothDeviceInfo& device) {
  for (BluetoothDeviceList::iterator it = list->begin(); it != list->end();
       ++it) {
    if ((*it).address == device.address) {
      *it = device;
      return;
    }
  }

  list->push_back(device);
}

// Removes the obsolete BluetoothDevices from |list|, if they are not in the
// |new_list|.
void RemoveObsoleteBluetoothDevicesFromList(
    BluetoothDeviceList* list,
    const std::set<std::string>& new_list) {
  for (BluetoothDeviceList::iterator it = list->begin(); it != list->end();
       ++it) {
    if (new_list.find((*it).address) == new_list.end()) {
      it = list->erase(it);
      if (it == list->end())
        return;
    }
  }
}

}  // namespace

class BluetoothDefaultView : public TrayItemMore {
 public:
  BluetoothDefaultView(SystemTrayItem* owner, bool show_more)
      : TrayItemMore(owner, show_more) {
    if (!MaterialDesignController::IsSystemTrayMenuMaterial()) {
      // The icon doesn't change in non-md.
      ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
      SetImage(
          *bundle.GetImageNamed(IDR_AURA_UBER_TRAY_BLUETOOTH).ToImageSkia());
    }
  }

  ~BluetoothDefaultView() override {}

  void Update() {
    SystemTrayDelegate* delegate = WmShell::Get()->system_tray_delegate();
    const bool enabled = delegate->GetBluetoothEnabled();
    if (delegate->GetBluetoothAvailable()) {
      ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
      const base::string16 label = rb.GetLocalizedString(
          enabled ? IDS_ASH_STATUS_TRAY_BLUETOOTH_ENABLED
                  : IDS_ASH_STATUS_TRAY_BLUETOOTH_DISABLED);
      SetLabel(label);
      SetAccessibleName(label);
      SetVisible(true);
    } else {
      SetVisible(false);
    }
    UpdateStyle();
  }

 protected:
  // TrayItemMore:
  std::unique_ptr<TrayPopupItemStyle> CreateStyle() const override {
    SystemTrayDelegate* delegate = WmShell::Get()->system_tray_delegate();
    std::unique_ptr<TrayPopupItemStyle> style = TrayItemMore::CreateStyle();
    style->set_color_style(
        delegate->GetBluetoothEnabled()
            ? TrayPopupItemStyle::ColorStyle::ACTIVE
            : delegate->GetBluetoothAvailable()
                  ? TrayPopupItemStyle::ColorStyle::INACTIVE
                  : TrayPopupItemStyle::ColorStyle::DISABLED);

    return style;
  }

  void UpdateStyle() override {
    TrayItemMore::UpdateStyle();

    if (!MaterialDesignController::IsSystemTrayMenuMaterial())
      return;

    SystemTrayDelegate* delegate = WmShell::Get()->system_tray_delegate();
    std::unique_ptr<TrayPopupItemStyle> style = CreateStyle();

    SetImage(gfx::CreateVectorIcon(delegate->GetBluetoothEnabled()
                                       ? kSystemMenuBluetoothIcon
                                       : kSystemMenuBluetoothDisabledIcon,
                                   style->GetForegroundColor()));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothDefaultView);
};

class BluetoothDetailedView : public TrayDetailsView {
 public:
  BluetoothDetailedView(SystemTrayItem* owner, LoginStatus login)
      : TrayDetailsView(owner),
        login_(login),
        manage_devices_(nullptr),
        throbber_(nullptr),
        toggle_bluetooth_(nullptr),
        enable_bluetooth_(nullptr),
        toggle_(nullptr) {
    CreateItems();
  }

  ~BluetoothDetailedView() override {
    // Stop discovering bluetooth devices when exiting BT detailed view.
    BluetoothStopDiscovering();
  }

  void Update() {
    BluetoothStartDiscovering();
    UpdateBluetoothDeviceList();

    // Update UI.
    UpdateDeviceScrollList();
    UpdateHeaderEntry();
    Layout();
  }

 private:
  void CreateItems() {
    CreateScrollableList();
    AppendSettingsEntries();
    CreateTitleRow(IDS_ASH_STATUS_TRAY_BLUETOOTH);
  }

  void BluetoothStartDiscovering() {
    // TODO(tdanderson|fukino): The material design version of the detailed
    // view should use an infinite loader bar instead of a throbber. See
    // crbug.com/632128.
    SystemTrayDelegate* delegate = WmShell::Get()->system_tray_delegate();
    if (delegate->GetBluetoothDiscovering()) {
      if (throbber_)
        throbber_->Start();
      return;
    }
    if (throbber_)
      throbber_->Stop();
    if (delegate->GetBluetoothEnabled())
      delegate->BluetoothStartDiscovering();
  }

  void BluetoothStopDiscovering() {
    SystemTrayDelegate* delegate = WmShell::Get()->system_tray_delegate();
    if (delegate && delegate->GetBluetoothDiscovering()) {
      delegate->BluetoothStopDiscovering();
      if (throbber_)
        throbber_->Stop();
    }
  }

  void UpdateBluetoothDeviceList() {
    std::set<std::string> new_connecting_devices;
    std::set<std::string> new_connected_devices;
    std::set<std::string> new_paired_not_connected_devices;
    std::set<std::string> new_discovered_not_paired_devices;

    BluetoothDeviceList list;
    WmShell::Get()->system_tray_delegate()->GetAvailableBluetoothDevices(&list);
    for (size_t i = 0; i < list.size(); ++i) {
      if (list[i].connecting) {
        list[i].display_name = l10n_util::GetStringFUTF16(
            IDS_ASH_STATUS_TRAY_BLUETOOTH_CONNECTING, list[i].display_name);
        new_connecting_devices.insert(list[i].address);
        UpdateBluetoothDeviceListHelper(&connecting_devices_, list[i]);
      } else if (list[i].connected && list[i].paired) {
        new_connected_devices.insert(list[i].address);
        UpdateBluetoothDeviceListHelper(&connected_devices_, list[i]);
      } else if (list[i].paired) {
        new_paired_not_connected_devices.insert(list[i].address);
        UpdateBluetoothDeviceListHelper(&paired_not_connected_devices_,
                                        list[i]);
      } else {
        new_discovered_not_paired_devices.insert(list[i].address);
        UpdateBluetoothDeviceListHelper(&discovered_not_paired_devices_,
                                        list[i]);
      }
    }
    RemoveObsoleteBluetoothDevicesFromList(&connecting_devices_,
                                           new_connecting_devices);
    RemoveObsoleteBluetoothDevicesFromList(&connected_devices_,
                                           new_connected_devices);
    RemoveObsoleteBluetoothDevicesFromList(&paired_not_connected_devices_,
                                           new_paired_not_connected_devices);
    RemoveObsoleteBluetoothDevicesFromList(&discovered_not_paired_devices_,
                                           new_discovered_not_paired_devices);
  }

  void UpdateHeaderEntry() {
    if (toggle_bluetooth_) {
      toggle_bluetooth_->SetToggled(
          !WmShell::Get()->system_tray_delegate()->GetBluetoothEnabled());
    }
  }

  void UpdateDeviceScrollList() {
    device_map_.clear();
    scroll_content()->RemoveAllChildViews(true);
    enable_bluetooth_ = nullptr;

    SystemTrayDelegate* delegate = WmShell::Get()->system_tray_delegate();
    bool bluetooth_enabled = delegate->GetBluetoothEnabled();
    bool blueooth_available = delegate->GetBluetoothAvailable();
    if (blueooth_available && !bluetooth_enabled && toggle_bluetooth_) {
      enable_bluetooth_ = AddScrollListItem(
          l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ENABLE_BLUETOOTH),
          false /* highlight */, false /* checked */, true /* enabled */);
    }

    AppendSameTypeDevicesToScrollList(connected_devices_, true, true,
                                      bluetooth_enabled);
    AppendSameTypeDevicesToScrollList(connecting_devices_, true, false,
                                      bluetooth_enabled);
    AppendSameTypeDevicesToScrollList(paired_not_connected_devices_, false,
                                      false, bluetooth_enabled);
    if (discovered_not_paired_devices_.size() > 0)
      AddScrollSeparator();
    AppendSameTypeDevicesToScrollList(discovered_not_paired_devices_, false,
                                      false, bluetooth_enabled);

    // Show user Bluetooth state if there is no bluetooth devices in list.
    if (device_map_.size() == 0) {
      if (blueooth_available && bluetooth_enabled) {
        AddScrollListItem(l10n_util::GetStringUTF16(
                              IDS_ASH_STATUS_TRAY_BLUETOOTH_DISCOVERING),
                          false /* highlight */, false /* checked */,
                          true /* enabled */);
      }
    }

    scroll_content()->SizeToPreferredSize();
  }

  void AppendSameTypeDevicesToScrollList(const BluetoothDeviceList& list,
                                         bool highlight,
                                         bool checked,
                                         bool enabled) {
    for (size_t i = 0; i < list.size(); ++i) {
      HoverHighlightView* container =
          AddScrollListItem(list[i].display_name, highlight, checked, enabled);
      device_map_[container] = list[i].address;
    }
  }

  HoverHighlightView* AddScrollListItem(const base::string16& text,
                                        bool highlight,
                                        bool checked,
                                        bool enabled) {
    HoverHighlightView* container = new HoverHighlightView(this);
    views::Label* label =
        container->AddCheckableLabel(text, highlight, checked);
    label->SetEnabled(enabled);
    scroll_content()->AddChildView(container);
    return container;
  }

  // Add settings entries.
  void AppendSettingsEntries() {
    if (!WmShell::Get()->system_tray_delegate()->ShouldShowSettings())
      return;

    // Add bluetooth device requires a browser window, hide it for non logged in
    // user.
    if (login_ == LoginStatus::NOT_LOGGED_IN || login_ == LoginStatus::LOCKED ||
        WmShell::Get()->GetSessionStateDelegate()->IsInSecondaryLoginScreen()) {
      return;
    }

    SystemTrayDelegate* delegate = WmShell::Get()->system_tray_delegate();
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    HoverHighlightView* container = new HoverHighlightView(this);
    container->AddLabel(
        rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_BLUETOOTH_MANAGE_DEVICES),
        gfx::ALIGN_LEFT, false /* highlight */);
    container->SetEnabled(delegate->GetBluetoothAvailable());
    AddChildView(container);
    manage_devices_ = container;
  }

  // Returns true if the device with |device_id| is found in |device_list|,
  // and the display_name of the device will be returned in |display_name| if
  // it's not NULL.
  bool FoundDevice(const std::string& device_id,
                   const BluetoothDeviceList& device_list,
                   base::string16* display_name) {
    for (size_t i = 0; i < device_list.size(); ++i) {
      if (device_list[i].address == device_id) {
        if (display_name)
          *display_name = device_list[i].display_name;
        return true;
      }
    }
    return false;
  }

  // Updates UI of the clicked bluetooth device to show it is being connected
  // or disconnected if such an operation is going to be performed underway.
  void UpdateClickedDevice(const std::string& device_id,
                           views::View* item_container) {
    base::string16 display_name;
    if (FoundDevice(device_id, paired_not_connected_devices_, &display_name)) {
      display_name = l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_BLUETOOTH_CONNECTING, display_name);

      item_container->RemoveAllChildViews(true);
      static_cast<HoverHighlightView*>(item_container)
          ->AddCheckableLabel(display_name, true /* highlight */, false);
      scroll_content()->SizeToPreferredSize();
      static_cast<views::View*>(scroller())->Layout();
    }
  }

  // TrayDetailsView:
  void HandleViewClicked(views::View* view) override {
    SystemTrayDelegate* delegate = WmShell::Get()->system_tray_delegate();
    if (view == manage_devices_) {
      delegate->ManageBluetoothDevices();
      owner()->system_tray()->CloseSystemBubble();
      return;
    }

    if (view == enable_bluetooth_) {
      WmShell::Get()->RecordUserMetricsAction(
          delegate->GetBluetoothEnabled() ? UMA_STATUS_AREA_BLUETOOTH_DISABLED
                                          : UMA_STATUS_AREA_BLUETOOTH_ENABLED);
      delegate->ToggleBluetooth();
      return;
    }

    if (!delegate->GetBluetoothEnabled())
      return;

    std::map<views::View*, std::string>::iterator find;
    find = device_map_.find(view);
    if (find == device_map_.end())
      return;

    const std::string device_id = find->second;
    if (FoundDevice(device_id, connecting_devices_, nullptr))
      return;

    UpdateClickedDevice(device_id, view);
    delegate->ConnectToBluetoothDevice(device_id);
  }

  void HandleButtonPressed(views::Button* sender,
                           const ui::Event& event) override {
    if (MaterialDesignController::IsSystemTrayMenuMaterial()) {
      // TODO(fukino): Make the toggle button functional.
      if (sender == toggle_)
        toggle_->SetIsOn(toggle_->is_on(), true);
      return;
    }

    SystemTrayDelegate* delegate = WmShell::Get()->system_tray_delegate();
    if (sender == toggle_bluetooth_)
      delegate->ToggleBluetooth();
    else
      NOTREACHED();
  }

  void CreateExtraTitleRowButtons() override {
    if (login_ == LoginStatus::LOCKED)
      return;

    if (MaterialDesignController::IsSystemTrayMenuMaterial()) {
      toggle_ = title_row()->AddToggleButton(this);
      return;
    }

    throbber_ = new ThrobberView;
    throbber_->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BLUETOOTH_DISCOVERING));
    title_row()->AddViewToRowNonMd(throbber_, false);

    // Do not allow toggling bluetooth in the lock screen.
    SystemTrayDelegate* delegate = WmShell::Get()->system_tray_delegate();
    toggle_bluetooth_ =
        new TrayPopupHeaderButton(this, IDR_AURA_UBER_TRAY_BLUETOOTH_ENABLED,
                                  IDR_AURA_UBER_TRAY_BLUETOOTH_DISABLED,
                                  IDR_AURA_UBER_TRAY_BLUETOOTH_ENABLED_HOVER,
                                  IDR_AURA_UBER_TRAY_BLUETOOTH_DISABLED_HOVER,
                                  IDS_ASH_STATUS_TRAY_BLUETOOTH);
    toggle_bluetooth_->SetToggled(!delegate->GetBluetoothEnabled());
    toggle_bluetooth_->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_DISABLE_BLUETOOTH));
    toggle_bluetooth_->SetToggledTooltipText(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ENABLE_BLUETOOTH));
    toggle_bluetooth_->EnableCanvasFlippingForRTLUI(false);
    title_row()->AddViewToRowNonMd(toggle_bluetooth_, true);
  }

  LoginStatus login_;

  std::map<views::View*, std::string> device_map_;
  views::View* manage_devices_;

  // Not used in material design.
  ThrobberView* throbber_;

  // Not used in material design.
  TrayPopupHeaderButton* toggle_bluetooth_;

  HoverHighlightView* enable_bluetooth_;
  BluetoothDeviceList connected_devices_;
  BluetoothDeviceList connecting_devices_;
  BluetoothDeviceList paired_not_connected_devices_;
  BluetoothDeviceList discovered_not_paired_devices_;

  // The on/off toggle button used in material design.
  views::ToggleButton* toggle_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothDetailedView);
};

}  // namespace tray

TrayBluetooth::TrayBluetooth(SystemTray* system_tray)
    : SystemTrayItem(system_tray, UMA_BLUETOOTH),
      default_(nullptr),
      detailed_(nullptr) {
  WmShell::Get()->system_tray_notifier()->AddBluetoothObserver(this);
}

TrayBluetooth::~TrayBluetooth() {
  WmShell::Get()->system_tray_notifier()->RemoveBluetoothObserver(this);
}

views::View* TrayBluetooth::CreateTrayView(LoginStatus status) {
  return NULL;
}

views::View* TrayBluetooth::CreateDefaultView(LoginStatus status) {
  CHECK(default_ == NULL);
  default_ =
      new tray::BluetoothDefaultView(this, status != LoginStatus::LOCKED);
  default_->Update();
  return default_;
}

views::View* TrayBluetooth::CreateDetailedView(LoginStatus status) {
  if (!WmShell::Get()->system_tray_delegate()->GetBluetoothAvailable())
    return NULL;
  WmShell::Get()->RecordUserMetricsAction(
      UMA_STATUS_AREA_DETAILED_BLUETOOTH_VIEW);
  CHECK(detailed_ == NULL);
  detailed_ = new tray::BluetoothDetailedView(this, status);
  detailed_->Update();
  return detailed_;
}

void TrayBluetooth::DestroyTrayView() {}

void TrayBluetooth::DestroyDefaultView() {
  default_ = NULL;
}

void TrayBluetooth::DestroyDetailedView() {
  detailed_ = NULL;
}

void TrayBluetooth::UpdateAfterLoginStatusChange(LoginStatus status) {}

void TrayBluetooth::OnBluetoothRefresh() {
  if (default_)
    default_->Update();
  else if (detailed_)
    detailed_->Update();
}

void TrayBluetooth::OnBluetoothDiscoveringChanged() {
  if (!detailed_)
    return;
  detailed_->Update();
}

}  // namespace ash
