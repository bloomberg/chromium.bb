// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/tray_power.h"

#include <utility>

#include "ash/accessibility/accessibility_delegate.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/resources/grit/ash_resources.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/date/date_view.h"
#include "ash/system/power/battery_notification.h"
#include "ash/system/power/dual_role_notification.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_item_view.h"
#include "ash/system/tray/tray_utils.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/time/time.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view.h"

using message_center::MessageCenter;
using message_center::Notification;

namespace ash {

namespace tray {

// This view is used only for the tray.
class PowerTrayView : public TrayItemView {
 public:
  explicit PowerTrayView(SystemTrayItem* owner) : TrayItemView(owner) {
    CreateImageView();
    UpdateImage();
  }

  ~PowerTrayView() override = default;

  // Overridden from views::View.
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    node_data->SetName(accessible_name_);
    node_data->role = ax::mojom::Role::kButton;
  }

  void UpdateStatus() {
    UpdateImage();
    SetVisible(PowerStatus::Get()->IsBatteryPresent());
  }

 private:
  void UpdateImage() {
    const PowerStatus::BatteryImageInfo& info =
        PowerStatus::Get()->GetBatteryImageInfo();
    // Only change the image when the info changes. http://crbug.com/589348
    if (info_ && info_->ApproximatelyEqual(info))
      return;
    image_view()->SetImage(PowerStatus::GetBatteryImage(
        info, kTrayIconSize, SkColorSetA(kTrayIconColor, 0x4C),
        kTrayIconColor));
  }

  base::string16 accessible_name_;
  base::Optional<PowerStatus::BatteryImageInfo> info_;

  DISALLOW_COPY_AND_ASSIGN(PowerTrayView);
};

}  // namespace tray

TrayPower::TrayPower(SystemTray* system_tray)
    : SystemTrayItem(system_tray, UMA_POWER) {
  PowerStatus::Get()->AddObserver(this);
}

TrayPower::~TrayPower() {
  PowerStatus::Get()->RemoveObserver(this);
}

views::View* TrayPower::CreateTrayView(LoginStatus status) {
  // There may not be enough information when this is created about whether
  // there is a battery or not. So always create this, and adjust visibility as
  // necessary.
  CHECK(power_tray_ == nullptr);
  power_tray_ = new tray::PowerTrayView(this);
  power_tray_->UpdateStatus();
  return power_tray_;
}

views::View* TrayPower::CreateDefaultView(LoginStatus status) {
  // Make sure icon status is up to date. (Also triggers stub activation).
  PowerStatus::Get()->RequestStatusUpdate();
  return nullptr;
}

void TrayPower::OnTrayViewDestroyed() {
  power_tray_ = nullptr;
}

void TrayPower::OnPowerStatusChanged() {
  if (power_tray_)
    power_tray_->UpdateStatus();
}

}  // namespace ash
