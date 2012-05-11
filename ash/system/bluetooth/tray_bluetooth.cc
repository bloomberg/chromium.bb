// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/tray_bluetooth.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_item_more.h"
#include "ash/system/tray/tray_views.h"
#include "grit/ash_strings.h"
#include "grit/ui_resources.h"
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
  explicit BluetoothDefaultView(SystemTrayItem* owner)
      : TrayItemMore(owner) {
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    SetImage(bundle.GetImageNamed(IDR_AURA_UBER_TRAY_BLUETOOTH).ToSkBitmap());
    UpdateLabel();
  }

  virtual ~BluetoothDefaultView() {}

  void UpdateLabel() {
    ash::SystemTrayDelegate* delegate =
        ash::Shell::GetInstance()->tray_delegate();
    if (delegate->GetBluetoothAvailable()) {
      ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
      SetLabel(rb.GetLocalizedString(delegate->GetBluetoothEnabled() ?
          IDS_ASH_STATUS_TRAY_BLUETOOTH_CONNECTED :
          IDS_ASH_STATUS_TRAY_BLUETOOTH_DISABLED));
      SetVisible(true);
    } else {
      SetVisible(false);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothDefaultView);
};

class BluetoothDetailedView : public views::View,
                              public ViewClickListener,
                              public views::ButtonListener {
 public:
  explicit BluetoothDetailedView(user::LoginStatus login)
      : login_(login),
        header_(NULL),
        add_device_(NULL),
        toggle_bluetooth_(NULL) {
    SetLayoutManager(new views::BoxLayout(
        views::BoxLayout::kVertical, 0, 0, 0));
    set_background(views::Background::CreateSolidBackground(kBackgroundColor));

    BluetoothDeviceList list;
    Shell::GetInstance()->tray_delegate()->GetAvailableBluetoothDevices(&list);
    Update(list);
  }

  virtual ~BluetoothDetailedView() {}

  void Update(const BluetoothDeviceList& list) {
    RemoveAllChildViews(true);

    header_ = NULL;
    add_device_ = NULL;
    toggle_bluetooth_ = NULL;

    AppendDeviceList(list);
    AppendSettingsEntries();
    AppendHeaderEntry();

    Layout();
  }

 private:
  void AppendHeaderEntry() {
    header_ = new SpecialPopupRow();
    header_->SetTextLabel(IDS_ASH_STATUS_TRAY_BLUETOOTH, this);
    AddChildView(header_);

    if (login_ == user::LOGGED_IN_LOCKED)
      return;

    // Do not allow toggling bluetooth in the lock screen.
    ash::SystemTrayDelegate* delegate =
        ash::Shell::GetInstance()->tray_delegate();
    toggle_bluetooth_ = new TrayPopupHeaderButton(this,
        IDR_AURA_UBER_TRAY_BLUETOOTH_ENABLED,
        IDR_AURA_UBER_TRAY_BLUETOOTH_DISABLED,
        IDR_AURA_UBER_TRAY_BLUETOOTH_ENABLED_HOVER,
        IDR_AURA_UBER_TRAY_BLUETOOTH_DISABLED_HOVER);
    toggle_bluetooth_->SetToggled(!delegate->GetBluetoothEnabled());
    header_->AddButton(toggle_bluetooth_);
  }

  void AppendDeviceList(const BluetoothDeviceList& list) {
    views::View* devices = new views::View;
    devices->SetLayoutManager(new views::BoxLayout(
        views::BoxLayout::kVertical, 0, 0, 1));
    device_map_.clear();

    for (size_t i = 0; i < list.size(); i++) {
      HoverHighlightView* container = new HoverHighlightView(this);
      container->set_fixed_height(kTrayPopupItemHeight);
      container->AddLabel(list[i].display_name,
          list[i].connected ? gfx::Font::BOLD : gfx::Font::NORMAL);
      devices->AddChildView(container);
      device_map_[container] = list[i].address;
    }

    FixedSizedScrollView* scroller = new FixedSizedScrollView;
    scroller->set_border(views::Border::CreateSolidSidedBorder(1, 0, 1, 0,
        SkColorSetARGB(25, 0, 0, 0)));
    scroller->set_fixed_size(
        gfx::Size(devices->GetPreferredSize().width() +
                  scroller->GetScrollBarWidth(),
                  kDeviceListHeight));
    scroller->SetContentsView(devices);
    AddChildView(scroller);
  }

  // Add settings entries.
  void AppendSettingsEntries() {
    // Add bluetooth device requires a browser window, hide it for non logged in
    // user.
    if (login_ == user::LOGGED_IN_NONE ||
        login_ == user::LOGGED_IN_LOCKED)
      return;

    ash::SystemTrayDelegate* delegate =
        ash::Shell::GetInstance()->tray_delegate();
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    HoverHighlightView* container = new HoverHighlightView(this);
    container->set_fixed_height(kTrayPopupItemHeight);
    container->AddLabel(rb.GetLocalizedString(
        IDS_ASH_STATUS_TRAY_BLUETOOTH_ADD_DEVICE), gfx::Font::NORMAL);
    container->SetEnabled(delegate->GetBluetoothEnabled());
    AddChildView(container);
    add_device_ = container;
  }

  // Overridden from ViewClickListener.
  virtual void ClickedOn(views::View* sender) OVERRIDE {
    ash::SystemTrayDelegate* delegate =
        ash::Shell::GetInstance()->tray_delegate();
    if (sender == header_->content()) {
      Shell::GetInstance()->tray()->ShowDefaultView();
    } else if (sender == add_device_) {
      delegate->AddBluetoothDevice();
    } else {
      std::map<views::View*, std::string>::iterator find;
      find = device_map_.find(sender);
      if (find != device_map_.end()) {
        std::string device_id = find->second;
        delegate->ToggleBluetoothConnection(device_id);
      }
    }
  }

  // Overridden from ButtonListener.
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE {
    ash::SystemTrayDelegate* delegate =
        ash::Shell::GetInstance()->tray_delegate();
    if (sender == toggle_bluetooth_)
      delegate->ToggleBluetooth();
    else
      NOTREACHED();
  }

  user::LoginStatus login_;

  std::map<views::View*, std::string> device_map_;
  SpecialPopupRow* header_;
  views::View* add_device_;
  TrayPopupHeaderButton* toggle_bluetooth_;
  views::View* settings_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothDetailedView);
};

}  // namespace tray

TrayBluetooth::TrayBluetooth()
    : default_(NULL),
      detailed_(NULL) {
}

TrayBluetooth::~TrayBluetooth() {
}

views::View* TrayBluetooth::CreateTrayView(user::LoginStatus status) {
  return NULL;
}

views::View* TrayBluetooth::CreateDefaultView(user::LoginStatus status) {
  CHECK(default_ == NULL);
  default_ = new tray::BluetoothDefaultView(this);
  return default_;
}

views::View* TrayBluetooth::CreateDetailedView(user::LoginStatus status) {
  if (!Shell::GetInstance()->tray_delegate()->GetBluetoothAvailable())
    return NULL;
  CHECK(detailed_ == NULL);
  detailed_ = new tray::BluetoothDetailedView(status);
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
  BluetoothDeviceList list;
  Shell::GetInstance()->tray_delegate()->GetAvailableBluetoothDevices(&list);
  if (default_)
    default_->UpdateLabel();
  else if (detailed_)
    detailed_->Update(list);
}

}  // namespace internal
}  // namespace ash
