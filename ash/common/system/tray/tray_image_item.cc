// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/tray_image_item.h"

#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/shelf/wm_shelf_util.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_item_view.h"
#include "ash/common/system/tray/tray_utils.h"
#include "ash/system/tray/system_tray.h"
#include "grit/ash_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Maps a non-MD PNG resource id to its corresponding MD vector icon id.
// TODO(tdanderson): Remove this once material design is enabled by
// default. See crbug.com/614453.
gfx::VectorIconId ResourceIdToVectorIconId(int resource_id) {
  gfx::VectorIconId vector_id = gfx::VectorIconId::VECTOR_ICON_NONE;
  switch (resource_id) {
    case IDR_AURA_UBER_TRAY_VOLUME_MUTE:
      return gfx::VectorIconId::SYSTEM_TRAY_VOLUME_MUTE;
    case IDR_AURA_UBER_TRAY_TRACING:
      // TODO(tdanderson): Update the icon used for tracing or remove it from
      // the system tray. See crbug.com/625691.
      return gfx::VectorIconId::CODE;
    case IDR_AURA_UBER_TRAY_ACCESSIBILITY:
      return gfx::VectorIconId::SYSTEM_TRAY_ACCESSIBILITY;
    case IDR_AURA_UBER_TRAY_UPDATE:
      return gfx::VectorIconId::SYSTEM_TRAY_UPDATE;
    case IDR_AURA_UBER_TRAY_AUTO_ROTATION_LOCKED:
      return gfx::VectorIconId::SYSTEM_TRAY_ROTATION_LOCK_LOCKED;
    case IDR_AURA_UBER_TRAY_CAPS_LOCK:
      return gfx::VectorIconId::SYSTEM_TRAY_CAPS_LOCK;
    default:
      NOTREACHED();
      break;
  }

  return vector_id;
}

}  // namespace

TrayImageItem::TrayImageItem(SystemTray* system_tray, int resource_id)
    : SystemTrayItem(system_tray),
      resource_id_(resource_id),
      tray_view_(NULL) {}

TrayImageItem::~TrayImageItem() {}

views::View* TrayImageItem::tray_view() {
  return tray_view_;
}

views::View* TrayImageItem::CreateTrayView(LoginStatus status) {
  CHECK(tray_view_ == NULL);
  tray_view_ = new TrayItemView(this);
  tray_view_->CreateImageView();

  if (MaterialDesignController::UseMaterialDesignSystemIcons()) {
    tray_view_->image_view()->SetImage(CreateVectorIcon(
        ResourceIdToVectorIconId(resource_id_), kTrayIconSize, kTrayIconColor));
  } else {
    tray_view_->image_view()->SetImage(ui::ResourceBundle::GetSharedInstance()
                                           .GetImageNamed(resource_id_)
                                           .ToImageSkia());
  }

  tray_view_->SetVisible(GetInitialVisibility());
  SetItemAlignment(system_tray()->shelf_alignment());
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
  SetItemAlignment(alignment);
}

void TrayImageItem::DestroyTrayView() {
  tray_view_ = NULL;
}

void TrayImageItem::DestroyDefaultView() {}

void TrayImageItem::DestroyDetailedView() {}

// TODO(tdanderson): Consider moving or renaming this function, as it is only
// used by TrayUpdate to modify the update severity. See crbug.com/625692.
void TrayImageItem::SetImageFromResourceId(int resource_id) {
  resource_id_ = resource_id;
  if (!tray_view_)
    return;
  tray_view_->image_view()->SetImage(ui::ResourceBundle::GetSharedInstance()
                                         .GetImageNamed(resource_id_)
                                         .ToImageSkia());
}

void TrayImageItem::SetItemAlignment(ShelfAlignment alignment) {
  // Center the item dependent on the orientation of the shelf.
  views::BoxLayout::Orientation layout = IsHorizontalAlignment(alignment)
                                             ? views::BoxLayout::kHorizontal
                                             : views::BoxLayout::kVertical;
  tray_view_->SetLayoutManager(new views::BoxLayout(layout, 0, 0, 0));
  tray_view_->Layout();
}

}  // namespace ash
