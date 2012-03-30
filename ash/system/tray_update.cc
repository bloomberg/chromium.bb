// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray_update.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_views.h"
#include "grit/ash_strings.h"
#include "grit/ui_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace {

class UpdateView : public views::View {
 public:
  UpdateView() {
    set_focusable(true);
    SetLayoutManager(new
        views::BoxLayout(views::BoxLayout::kHorizontal,
        ash::kTrayPopupPaddingHorizontal, 0,
        ash::kTrayPopupPaddingBetweenItems));

    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    views::ImageView* image =
        new ash::internal::FixedSizedImageView(0, ash::kTrayPopupItemHeight);
    image->SetImage(bundle.GetImageNamed(IDR_AURA_UBER_TRAY_UPDATE_DARK).
        ToSkBitmap());

    AddChildView(image);
    AddChildView(new views::Label(
        bundle.GetLocalizedString(IDS_ASH_STATUS_TRAY_UPDATE)));
  }

  virtual ~UpdateView() {}

 private:
  // Overridden from views::View.
  virtual bool OnKeyPressed(const views::KeyEvent& event) OVERRIDE {
    if (event.key_code() == ui::VKEY_SPACE ||
        event.key_code() == ui::VKEY_RETURN) {
      ash::Shell::GetInstance()->tray_delegate()->RequestRestart();
      return true;
    }
    return false;
  }

  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE {
    ash::Shell::GetInstance()->tray_delegate()->RequestRestart();
    return true;
  }

  DISALLOW_COPY_AND_ASSIGN(UpdateView);
};

}

namespace ash {
namespace internal {

TrayUpdate::TrayUpdate()
    : TrayImageItem(IDR_AURA_UBER_TRAY_UPDATE) {
}

TrayUpdate::~TrayUpdate() {}

bool TrayUpdate::GetInitialVisibility() {
  return Shell::GetInstance()->tray_delegate()->SystemShouldUpgrade();
}

views::View* TrayUpdate::CreateDefaultView(user::LoginStatus status) {
  if (!Shell::GetInstance()->tray_delegate()->SystemShouldUpgrade())
    return NULL;
  default_.reset(new UpdateView);
  return default_.get();
}

void TrayUpdate::DestroyDefaultView() {
  default_.reset();
}

void TrayUpdate::OnUpdateRecommended() {
  image_view()->SetVisible(true);
}

}  // namespace internal
}  // namespace ash
