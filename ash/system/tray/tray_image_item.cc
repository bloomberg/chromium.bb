// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_image_item.h"

#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/image_view.h"

namespace ash {
namespace internal {

TrayImageItem::TrayImageItem(int resource_id)
    : resource_id_(resource_id) {
}

TrayImageItem::~TrayImageItem() {}

views::View* TrayImageItem::CreateTrayView(user::LoginStatus status) {
  image_view_.reset(new views::ImageView);
  image_view_->SetImage(ui::ResourceBundle::GetSharedInstance().
      GetImageNamed(resource_id_).ToSkBitmap());
  image_view_->SetVisible(GetInitialVisibility());
  return image_view_.get();
}

views::View* TrayImageItem::CreateDefaultView(user::LoginStatus status) {
  return NULL;
}

views::View* TrayImageItem::CreateDetailedView(user::LoginStatus status) {
  return NULL;
}

void TrayImageItem::DestroyTrayView() {
  image_view_.reset();
}

void TrayImageItem::DestroyDefaultView() {
}

void TrayImageItem::DestroyDetailedView() {
}

}  // namespace internal
}  // namespace ash
