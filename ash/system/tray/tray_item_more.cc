// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_item_more.h"

#include "ash/system/tray/fixed_sized_image_view.h"
#include "ash/system/tray/system_tray_item.h"
#include "ash/system/tray/tray_constants.h"
#include "grit/ash_resources.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

TrayItemMore::TrayItemMore(SystemTrayItem* owner, bool show_more)
    : owner_(owner),
      show_more_(show_more),
      icon_(NULL),
      label_(NULL),
      more_(NULL) {
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal,
      kTrayPopupPaddingHorizontal, 0, kTrayPopupPaddingBetweenItems));

  icon_ = new FixedSizedImageView(0, kTrayPopupItemHeight);
  AddChildView(icon_);

  label_ = new views::Label;
  label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(label_);

  if (show_more) {
    more_ = new views::ImageView;
    more_->EnableCanvasFlippingForRTLUI(true);
    more_->SetImage(ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        IDR_AURA_UBER_TRAY_MORE).ToImageSkia());
    AddChildView(more_);
  }
}

TrayItemMore::~TrayItemMore() {
}

void TrayItemMore::SetLabel(const base::string16& label) {
  label_->SetText(label);
  Layout();
  SchedulePaint();
}

void TrayItemMore::SetImage(const gfx::ImageSkia* image_skia) {
  icon_->SetImage(image_skia);
  SchedulePaint();
}

void TrayItemMore::SetAccessibleName(const base::string16& name) {
  accessible_name_ = name;
}

void TrayItemMore::ReplaceIcon(views::View* view) {
  delete icon_;
  icon_ = NULL;
  AddChildViewAt(view, 0);
}

bool TrayItemMore::PerformAction(const ui::Event& event) {
  if (!show_more_)
    return false;

  owner()->TransitionDetailedView();
  return true;
}

void TrayItemMore::Layout() {
  // Let the box-layout do the layout first. Then move the '>' arrow to right
  // align.
  views::View::Layout();

  if (!show_more_)
    return;

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

void TrayItemMore::GetAccessibleState(ui::AXViewState* state) {
  state->role = ui::AX_ROLE_BUTTON;
  state->name = accessible_name_;
}

}  // namespace ash
