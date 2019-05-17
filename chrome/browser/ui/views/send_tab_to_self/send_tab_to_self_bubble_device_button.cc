// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_device_button.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "components/sync/protocol/sync.pb.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"

namespace send_tab_to_self {

namespace {

// Icon sizes in DIP.
constexpr int kPrimaryIconSize = 20;

enum class DeviceIconType {
  DESKTOP = 0,
  PHONE = 1,
  TOTAL_COUNT = 2  // Add new types above this line.
};

gfx::ImageSkia CreateDeviceIcon(DeviceIconType icon_type, bool enabled = true) {
  const gfx::VectorIcon* vector_icon;
  switch (icon_type) {
    case DeviceIconType::DESKTOP:
      vector_icon = &kHardwareComputerIcon;
      break;
    case DeviceIconType::PHONE:
      vector_icon = &kHardwareSmartphoneIcon;
      break;
    default:
      vector_icon = &kSendTabToSelfIcon;
  }
  SkColor icon_color = enabled ? gfx::kChromeIconGrey : gfx::kGoogleGrey500;
  return gfx::CreateVectorIcon(*vector_icon, kPrimaryIconSize, icon_color);
}

gfx::ImageSkia CreateIconView(
    const sync_pb::SyncEnums::DeviceType device_type) {
  if (device_type == sync_pb::SyncEnums::TYPE_PHONE) {
    return CreateDeviceIcon(DeviceIconType::PHONE);
  }
  return CreateDeviceIcon(DeviceIconType::DESKTOP);
}

}  // namespace

SendTabToSelfBubbleDeviceButton::SendTabToSelfBubbleDeviceButton(
    views::ButtonListener* button_listener,
    const std::string& device_name,
    const TargetDeviceInfo& device_info,
    int button_tag)
    : HoverButton(button_listener,
                  CreateIconView(device_info.device_type),
                  base::UTF8ToUTF16(device_name)) {
  device_name_ = device_name;
  device_guid_ = device_info.cache_guid;
  device_type_ = device_info.device_type;
  set_tag(button_tag);
  SetEnabled(true);
}

SendTabToSelfBubbleDeviceButton::~SendTabToSelfBubbleDeviceButton() = default;

}  // namespace send_tab_to_self
