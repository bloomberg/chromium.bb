// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_views.h"

#include "ash/system/tray/tray_constants.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {
namespace internal {

namespace {
const int kIconPaddingLeft = 5;
}

////////////////////////////////////////////////////////////////////////////////
// FixedWidthImageView

FixedWidthImageView::FixedWidthImageView() {
  SetHorizontalAlignment(views::ImageView::CENTER);
  SetVerticalAlignment(views::ImageView::CENTER);
}

FixedWidthImageView::~FixedWidthImageView() {
}

gfx::Size FixedWidthImageView::GetPreferredSize() {
  gfx::Size size = views::ImageView::GetPreferredSize();
  return gfx::Size(kTrayPopupDetailsIconWidth, size.height());
}

////////////////////////////////////////////////////////////////////////////////
// HoverHighlightView

HoverHighlightView::HoverHighlightView(ViewClickListener* listener)
    : listener_(listener) {
  set_notify_enter_exit_on_child(true);
}

HoverHighlightView::~HoverHighlightView() {
}

void HoverHighlightView::AddIconAndLabel(const SkBitmap& image,
                                         const string16& text,
                                         gfx::Font::FontStyle style) {
  SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kHorizontal, 0, 3, kIconPaddingLeft));
  views::ImageView* image_view = new FixedWidthImageView;
  image_view->SetImage(image);
  AddChildView(image_view);

  views::Label* label = new views::Label(text);
  label->SetFont(label->font().DeriveFont(0, style));
  AddChildView(label);
}

void HoverHighlightView::AddLabel(const string16& text) {
  SetLayoutManager(new views::FillLayout());
  views::Label* label = new views::Label(text);
  label->set_border(views::Border::CreateEmptyBorder(
      5, kTrayPopupDetailsIconWidth + kIconPaddingLeft, 5, 0));
  label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  AddChildView(label);
}

bool HoverHighlightView::OnMousePressed(const views::MouseEvent& event) {
  if (!listener_)
    return false;
  listener_->ClickedOn(this);
  return true;
}

void HoverHighlightView::OnMouseEntered(const views::MouseEvent& event) {
  set_background(views::Background::CreateSolidBackground(
      ash::kHoverBackgroundColor));
  SchedulePaint();
}

void HoverHighlightView::OnMouseExited(const views::MouseEvent& event) {
  set_background(NULL);
  SchedulePaint();
}

}  // namespace internal
}  // namespace ash
