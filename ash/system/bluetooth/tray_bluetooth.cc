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
const int kDeviceListHeight = 190;
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
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    SetLabel(rb.GetLocalizedString(delegate->GetBluetoothEnabled() ?
        IDS_ASH_STATUS_TRAY_BLUETOOTH_CONNECTED :
        IDS_ASH_STATUS_TRAY_BLUETOOTH_DISABLED));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothDefaultView);
};

class BluetoothDetailedView : public views::View,
                              public ViewClickListener {
 public:
  explicit BluetoothDetailedView(user::LoginStatus login)
      : login_(login),
        header_(NULL),
        add_device_(NULL),
        toggle_bluetooth_(NULL) {
    SetLayoutManager(new views::BoxLayout(
        views::BoxLayout::kVertical, 1, 1, 1));
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

    AppendHeaderEntry();
    AppendDeviceList(list);
    AppendSettingsEntries();

    Layout();
  }

 private:
  void AppendHeaderEntry() {
    header_ = CreateDetailedHeaderEntry(IDS_ASH_STATUS_TRAY_BLUETOOTH, this);
    AddChildView(header_);
  }

  void AppendDeviceList(const BluetoothDeviceList& list) {
    views::View* devices = new views::View;
    devices->SetLayoutManager(new views::BoxLayout(
        views::BoxLayout::kVertical, 0, 0, 1));
    device_map_.clear();

    for (size_t i = 0; i < list.size(); i++) {
      HoverHighlightView* container = new HoverHighlightView(this);
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
    // If screen is locked, hide all settings entries as user should not be able
    // to modify state.
    if (login_ == user::LOGGED_IN_LOCKED)
      return;

    ash::SystemTrayDelegate* delegate =
        ash::Shell::GetInstance()->tray_delegate();
    HoverHighlightView* container = new HoverHighlightView(this);
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    container->AddLabel(rb.GetLocalizedString(
        delegate->GetBluetoothEnabled() ?
            IDS_ASH_STATUS_TRAY_DISABLE_BLUETOOTH :
            IDS_ASH_STATUS_TRAY_ENABLE_BLUETOOTH), gfx::Font::NORMAL);
    AddChildView(container);
    toggle_bluetooth_ = container;

    // Add bluetooth device requires a browser window, hide it for non logged in
    // user.
    if (login_ != user::LOGGED_IN_NONE) {
      container = new HoverHighlightView(this);
      container->AddLabel(rb.GetLocalizedString(
            IDS_ASH_STATUS_TRAY_BLUETOOTH_ADD_DEVICE), gfx::Font::NORMAL);
      AddChildView(container);
      add_device_ = container;
    }
  }

  // Overridden from ViewClickListener.
  virtual void ClickedOn(views::View* sender) OVERRIDE {
    ash::SystemTrayDelegate* delegate =
        ash::Shell::GetInstance()->tray_delegate();
    if (sender == header_) {
      Shell::GetInstance()->tray()->ShowDefaultView();
    } else if (sender == toggle_bluetooth_) {
      delegate->ToggleBluetooth();
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

  user::LoginStatus login_;

  std::map<views::View*, std::string> device_map_;
  views::View* header_;
  views::View* add_device_;
  views::View* toggle_bluetooth_;
  views::View* settings_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothDetailedView);
};

}  // namespace tray

TrayBluetooth::TrayBluetooth() {
}

TrayBluetooth::~TrayBluetooth() {
}

views::View* TrayBluetooth::CreateTrayView(user::LoginStatus status) {
  return NULL;
}

views::View* TrayBluetooth::CreateDefaultView(user::LoginStatus status) {
  if (!Shell::GetInstance()->tray_delegate()->GetBluetoothAvailable())
    return NULL;
  default_.reset(new tray::BluetoothDefaultView(this));
  return default_.get();
}

views::View* TrayBluetooth::CreateDetailedView(user::LoginStatus status) {
  if (!Shell::GetInstance()->tray_delegate()->GetBluetoothAvailable())
    return NULL;
  detailed_.reset(new tray::BluetoothDetailedView(status));
  return detailed_.get();
}

void TrayBluetooth::DestroyTrayView() {
}

void TrayBluetooth::DestroyDefaultView() {
  default_.reset();
}

void TrayBluetooth::DestroyDetailedView() {
  detailed_.reset();
}

void TrayBluetooth::OnBluetoothRefresh() {
  BluetoothDeviceList list;
  Shell::GetInstance()->tray_delegate()->GetAvailableBluetoothDevices(&list);
  if (default_.get())
    default_->UpdateLabel();
  if (detailed_.get())
    detailed_->Update(list);
}

}  // namespace internal
}  // namespace ash
