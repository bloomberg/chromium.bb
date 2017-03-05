// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/special_popup_row.h"

#include "ash/common/ash_constants.h"
#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/system/tray/hover_highlight_view.h"
#include "ash/common/system/tray/throbber_view.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_popup_header_button.h"
#include "ash/resources/grit/ash_resources.h"
#include "ash/strings/grit/ash_strings.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/painter.h"

namespace ash {
namespace {

const int kIconPaddingLeft = 5;
const int kSeparatorInset = 10;
const int kSpecialPopupRowHeight = 55;
const int kBorderHeight = 1;
const SkColor kBorderColor = SkColorSetRGB(0xaa, 0xaa, 0xaa);

views::View* CreateViewContainer() {
  views::View* view = new views::View;
  view->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0));
  view->SetBorder(views::CreateEmptyBorder(4, 0, 4, 5));
  return view;
}

}  // namespace

SpecialPopupRow::SpecialPopupRow()
    : content_(nullptr), views_after_content_container_(nullptr) {
  DCHECK(!MaterialDesignController::IsSystemTrayMenuMaterial());
  set_background(
      views::Background::CreateSolidBackground(kHeaderBackgroundColor));
  SetBorder(
      views::CreateSolidSidedBorder(kBorderHeight, 0, 0, 0, kBorderColor));
}

SpecialPopupRow::~SpecialPopupRow() {}

void SpecialPopupRow::SetTextLabel(int string_id, ViewClickListener* listener) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  HoverHighlightView* container = new HoverHighlightView(listener);
  container->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kHorizontal, 0, 3, kIconPaddingLeft));

  container->set_highlight_color(SkColorSetARGB(0, 0, 0, 0));
  container->set_default_color(SkColorSetARGB(0, 0, 0, 0));
  container->set_text_highlight_color(kHeaderTextColorHover);
  container->set_text_default_color(kHeaderTextColorNormal);

  container->AddIconAndLabel(
      *rb.GetImageNamed(IDR_AURA_UBER_TRAY_LESS).ToImageSkia(),
      rb.GetLocalizedString(string_id), true /* highlight */);

  container->SetBorder(
      views::CreateEmptyBorder(0, kTrayPopupPaddingHorizontal, 0, 0));

  container->SetAccessibleName(
      rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_PREVIOUS_MENU));
  SetContent(container);
}

void SpecialPopupRow::SetContent(views::View* view) {
  CHECK(!content_);
  views::BoxLayout* box_layout =
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0);
  SetLayoutManager(box_layout);
  content_ = view;
  AddChildViewAt(content_, 0);
}

void SpecialPopupRow::AddViewToTitleRow(views::View* view) {
  AddViewAfterContent(view);
}

void SpecialPopupRow::AddViewToRowNonMd(views::View* view, bool add_separator) {
  AddViewAfterContent(view, add_separator);
}

gfx::Size SpecialPopupRow::GetPreferredSize() const {
  gfx::Size size = views::View::GetPreferredSize();
  size.set_height(kSpecialPopupRowHeight);
  return size;
}

int SpecialPopupRow::GetHeightForWidth(int width) const {
  return kSpecialPopupRowHeight;
}

void SpecialPopupRow::Layout() {
  views::View::Layout();

  const gfx::Rect content_bounds = GetContentsBounds();
  if (content_bounds.IsEmpty())
    return;

  if (!views_after_content_container_) {
    content_->SetBoundsRect(GetContentsBounds());
    return;
  }

  gfx::Rect bounds(views_after_content_container_->GetPreferredSize());
  bounds.set_height(content_bounds.height());

  gfx::Rect container_bounds = content_bounds;
  container_bounds.ClampToCenteredSize(bounds.size());
  container_bounds.set_x(content_bounds.width() - container_bounds.width());
  views_after_content_container_->SetBoundsRect(container_bounds);

  bounds = content_->bounds();
  bounds.set_width(views_after_content_container_->x());
  content_->SetBoundsRect(bounds);
}

void SpecialPopupRow::AddViewAfterContent(views::View* view) {
  AddViewAfterContent(view, false);
}

void SpecialPopupRow::AddViewAfterContent(views::View* view,
                                          bool add_separator) {
  if (!views_after_content_container_) {
    views_after_content_container_ = CreateViewContainer();
    AddChildView(views_after_content_container_);
  }

  if (add_separator) {
    views::Separator* separator = new views::Separator();
    separator->SetColor(ash::kBorderDarkColor);
    separator->SetBorder(
        views::CreateEmptyBorder(kSeparatorInset, 0, kSeparatorInset, 0));
    views_after_content_container_->AddChildView(separator);
  }

  views_after_content_container_->AddChildView(view);
}

}  // namespace ash
