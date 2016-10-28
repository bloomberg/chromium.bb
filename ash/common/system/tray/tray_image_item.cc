// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/tray_image_item.h"

#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/shelf/wm_shelf_util.h"
#include "ash/common/system/tray/system_tray.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_item_view.h"
#include "ash/common/system/tray/tray_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "grit/ash_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Maps a non-MD PNG resource id to its corresponding MD vector icon.
// TODO(tdanderson): Remove this once material design is enabled by
// default. See crbug.com/614453.
const gfx::VectorIcon& ResourceIdToVectorIcon(int resource_id) {
  switch (resource_id) {
    case IDR_AURA_UBER_TRAY_ACCESSIBILITY:
      return kSystemTrayAccessibilityIcon;
    case IDR_AURA_UBER_TRAY_UPDATE:
      return kSystemTrayUpdateIcon;
    case IDR_AURA_UBER_TRAY_VOLUME_MUTE:
      return kSystemTrayVolumeMuteIcon;
#if defined(OS_CHROMEOS)
    case IDR_AURA_UBER_TRAY_AUTO_ROTATION_LOCKED:
      return kSystemTrayRotationLockLockedIcon;
    case IDR_AURA_UBER_TRAY_CAPS_LOCK:
      return kSystemTrayCapsLockIcon;
    case IDR_AURA_UBER_TRAY_TRACING:
      // TODO(tdanderson): Update the icon used for tracing or remove it from
      // the system tray. See crbug.com/625691.
      return kSystemMenuTimerIcon;
#endif
    default:
      NOTREACHED();
      break;
  }

  return gfx::kNoneIcon;
}

}  // namespace

TrayImageItem::TrayImageItem(SystemTray* system_tray,
                             int resource_id,
                             UmaType uma_type)
    : SystemTrayItem(system_tray, uma_type),
      resource_id_(resource_id),
      icon_color_(kTrayIconColor),
      tray_view_(nullptr) {}

TrayImageItem::~TrayImageItem() {}

views::View* TrayImageItem::tray_view() {
  return tray_view_;
}

views::View* TrayImageItem::CreateTrayView(LoginStatus status) {
  CHECK(!tray_view_);
  tray_view_ = new TrayItemView(this);
  tray_view_->CreateImageView();
  UpdateImageOnImageView();
  tray_view_->SetVisible(GetInitialVisibility());
  return tray_view_;
}

views::View* TrayImageItem::CreateDefaultView(LoginStatus status) {
  return nullptr;
}

views::View* TrayImageItem::CreateDetailedView(LoginStatus status) {
  return nullptr;
}

void TrayImageItem::UpdateAfterLoginStatusChange(LoginStatus status) {}

void TrayImageItem::UpdateAfterShelfAlignmentChange(ShelfAlignment alignment) {
  SetTrayImageItemBorder(tray_view_, alignment);
}

void TrayImageItem::DestroyTrayView() {
  tray_view_ = nullptr;
}

void TrayImageItem::DestroyDefaultView() {}

void TrayImageItem::DestroyDetailedView() {}

void TrayImageItem::SetIconColor(SkColor color) {
  icon_color_ = color;
  UpdateImageOnImageView();
}

void TrayImageItem::SetImageFromResourceId(int resource_id) {
  resource_id_ = resource_id;
  UpdateImageOnImageView();
}

void TrayImageItem::UpdateImageOnImageView() {
  if (!tray_view_)
    return;

  if (MaterialDesignController::UseMaterialDesignSystemIcons()) {
    tray_view_->image_view()->SetImage(gfx::CreateVectorIcon(
        ResourceIdToVectorIcon(resource_id_), kTrayIconSize, icon_color_));
  } else {
    tray_view_->image_view()->SetImage(ui::ResourceBundle::GetSharedInstance()
                                           .GetImageNamed(resource_id_)
                                           .ToImageSkia());
  }
}

}  // namespace ash
