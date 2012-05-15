// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_item_more.h"

#include "ash/system/tray/system_tray_item.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_views.h"
#include "grit/ui_resources.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {
namespace internal {

TrayItemMore::TrayItemMore(SystemTrayItem* owner)
    : owner_(owner) {
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal,
      kTrayPopupPaddingHorizontal, 0, kTrayPopupPaddingBetweenItems));

  icon_ = new FixedSizedImageView(0, kTrayPopupItemHeight);
  AddChildView(icon_);

  label_ = new views::Label;
  AddChildView(label_);

  more_ = new views::ImageView;
  more_->EnableCanvasFlippingForRTLUI(true);
  more_->SetImage(ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      IDR_AURA_UBER_TRAY_MORE).ToSkBitmap());
  AddChildView(more_);
}

TrayItemMore::~TrayItemMore() {
}

void TrayItemMore::SetLabel(const string16& label) {
  label_->SetText(label);
  Layout();
  SchedulePaint();
}

void TrayItemMore::SetImage(const SkBitmap* bitmap) {
  icon_->SetImage(bitmap);
  SchedulePaint();
}

void TrayItemMore::SetAccessibleName(const string16& name) {
  accessible_name_ = name;
}

void TrayItemMore::ReplaceIcon(views::View* view) {
  delete icon_;
  icon_ = NULL;
  AddChildViewAt(view, 0);
}

bool TrayItemMore::PerformAction(const views::Event& event) {
  owner_->TransitionDetailedView();
  return true;
}

void TrayItemMore::Layout() {
  // Let the box-layout do the layout first. Then move the '>' arrow to right
  // align.
  views::View::Layout();

  // Make sure the chevron always has the full size.
  gfx::Size size = more_->GetPreferredSize();
  gfx::Rect bounds(size);
  bounds.set_x(width() - size.width() - kTrayPopupPaddingBetweenItems);
  bounds.set_y((height() - size.height()) / 2);
  more_->SetBoundsRect(bounds);

  // Adjust the label's bounds in case it got cut off by |more_|.
  if (label_->bounds().Intersects(more_->bounds())) {
    gfx::Rect bounds = label_->bounds();
    bounds.set_width(more_->x() - kTrayPopupPaddingBetweenItems - label_->x());
    label_->SetBoundsRect(bounds);
  }
}

void TrayItemMore::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_PUSHBUTTON;
  state->name = accessible_name_;
}

}  // namespace internal
}  // namespace ash
