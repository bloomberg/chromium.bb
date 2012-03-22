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

void HoverHighlightView::AddLabel(const string16& text,
                                  gfx::Font::FontStyle style) {
  SetLayoutManager(new views::FillLayout());
  views::Label* label = new views::Label(text);
  label->set_border(views::Border::CreateEmptyBorder(
      5, kTrayPopupDetailsIconWidth + kIconPaddingLeft, 5, 0));
  label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  label->SetFont(label->font().DeriveFont(0, style));
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

////////////////////////////////////////////////////////////////////////////////
// FixedSizedScrollView

FixedSizedScrollView::FixedSizedScrollView() {
  set_focusable(true);
  set_notify_enter_exit_on_child(true);
}

FixedSizedScrollView::~FixedSizedScrollView() {}

void FixedSizedScrollView::SetContentsView(View* view) {
  SetContents(view);
  view->SetBoundsRect(gfx::Rect(view->GetPreferredSize()));
}

gfx::Size FixedSizedScrollView::GetPreferredSize() {
  return fixed_size_;
}

void FixedSizedScrollView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  views::View* contents = GetContents();
  gfx::Rect bounds = contents->bounds();
  bounds.set_width(width() - GetScrollBarWidth());
  contents->SetBoundsRect(bounds);
}

void FixedSizedScrollView::OnMouseEntered(const views::MouseEvent& event) {
  // TODO(sad): This is done to make sure that the scroll view scrolls on
  // mouse-wheel events. This is ugly, and Ben thinks this is weird. There
  // should be a better fix for this.
  RequestFocus();
}

void FixedSizedScrollView::OnPaintFocusBorder(gfx::Canvas* canvas) {
  // Do not paint the focus border.
}

void SetupLabelForTray(views::Label* label) {
  label->SetFont(
      label->font().DeriveFont(2, gfx::Font::BOLD));
  label->SetAutoColorReadabilityEnabled(false);
  label->SetEnabledColor(SK_ColorWHITE);
  label->SetBackgroundColor(SkColorSetARGB(0, 255, 255, 255));
}

}  // namespace internal
}  // namespace ash
