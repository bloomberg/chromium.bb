// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_item_more.h"

#include "ash/system/tray/system_tray_item.h"
#include "ash/system/tray/tray_constants.h"
#include "grit/ui_resources.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/image_view.h"

namespace ash {
namespace internal {

TrayItemMore::TrayItemMore(SystemTrayItem* owner)
    : owner_(owner),
      more_(NULL) {
  set_focusable(true);
}

TrayItemMore::~TrayItemMore() {
}

void TrayItemMore::AddMore() {
  more_ = new views::ImageView;
  more_->SetImage(ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      IDR_AURA_UBER_TRAY_MORE).ToSkBitmap());
  AddChildView(more_);
}

void TrayItemMore::SetAccessibleName(const string16& name) {
  accessible_name_ = name;
}

void TrayItemMore::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_PUSHBUTTON;
  state->name = accessible_name_;
}

void TrayItemMore::Layout() {
  // Let the box-layout do the layout first. Then move the '>' arrow to right
  // align.
  views::View::Layout();

  gfx::Rect bounds = more_->bounds();
  bounds.set_x(width() - more_->width() - kTrayPopupPaddingBetweenItems);
  more_->SetBoundsRect(bounds);
}

bool TrayItemMore::OnKeyPressed(const views::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_SPACE ||
      event.key_code() == ui::VKEY_RETURN) {
    owner_->PopupDetailedView(0, true);
    return true;
  }
  return false;
}

bool TrayItemMore::OnMousePressed(const views::MouseEvent& event) {
  owner_->PopupDetailedView(0, true);
  return true;
}

}  // namespace internal
}  // namespace ash
