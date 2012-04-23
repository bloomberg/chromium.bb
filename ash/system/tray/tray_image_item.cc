// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_image_item.h"

#include "ash/system/tray/tray_item_view.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {
namespace internal {

TrayImageItem::TrayImageItem(int resource_id)
    : resource_id_(resource_id) {
}

TrayImageItem::~TrayImageItem() {}

views::View* TrayImageItem::tray_view() {
  return tray_view_.get();
}

views::View* TrayImageItem::CreateTrayView(user::LoginStatus status) {
  tray_view_.reset(new TrayItemView);
  tray_view_->CreateImageView();
  tray_view_->image_view()->SetImage(ui::ResourceBundle::GetSharedInstance().
      GetImageNamed(resource_id_).ToSkBitmap());
  tray_view_->SetVisible(GetInitialVisibility());
  return tray_view_.get();
}

views::View* TrayImageItem::CreateDefaultView(user::LoginStatus status) {
  return NULL;
}

views::View* TrayImageItem::CreateDetailedView(user::LoginStatus status) {
  return NULL;
}

void TrayImageItem::DestroyTrayView() {
  tray_view_.reset();
}

void TrayImageItem::DestroyDefaultView() {
}

void TrayImageItem::DestroyDetailedView() {
}

}  // namespace internal
}  // namespace ash
