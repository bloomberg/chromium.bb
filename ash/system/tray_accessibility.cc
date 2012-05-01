// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray_accessibility.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "grit/ui_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {

TrayAccessibility::TrayAccessibility()
    : TrayImageItem(IDR_AURA_UBER_TRAY_ACCESSIBILITY),
      detailed_(NULL),
      string_id_(0) {
}

TrayAccessibility::~TrayAccessibility() {}

bool TrayAccessibility::GetInitialVisibility() {
  return ash::Shell::GetInstance()->tray_delegate()->IsInAccessibilityMode();
}

views::View* TrayAccessibility::CreateDetailedView(user::LoginStatus status) {
  DCHECK(string_id_);
  CHECK(detailed_ == NULL);
  detailed_ = new views::View;

  detailed_->SetLayoutManager(new
      views::BoxLayout(views::BoxLayout::kHorizontal,
      kTrayPopupPaddingHorizontal, 10, kTrayPopupPaddingBetweenItems));

  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  views::ImageView* image = new views::ImageView;
  image->SetImage(bundle.GetImageNamed(IDR_AURA_UBER_TRAY_ACCESSIBILITY).
      ToSkBitmap());

  detailed_->AddChildView(image);
  detailed_->AddChildView(new views::Label(
        bundle.GetLocalizedString(string_id_)));

  return detailed_;
}

void TrayAccessibility::DestroyDetailedView() {
  detailed_ = NULL;
}

void TrayAccessibility::OnAccessibilityModeChanged(bool enabled,
                                                   int string_id) {
  if (tray_view())
    tray_view()->SetVisible(enabled);

  if (enabled) {
    string_id_ = string_id;
    PopupDetailedView(kTrayPopupAutoCloseDelayForTextInSeconds, false);
  } else if (detailed_) {
    string_id_ = 0;
    detailed_->GetWidget()->Close();
  }
}

}  // namespace internal
}  // namespace ash
