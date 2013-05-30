// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/painter.h"


namespace {
// Amount of padding at the edges of the bubble.
//
// This can't be statically initialized because
// LocationBarView::GetItemPadding() depends on whether we are using desktop or
// touch layout, and this in turn depends on the command line.
int GetBubbleOuterPadding() {
  return LocationBarView::GetItemPadding() - LocationBarView::kBubblePadding;
}
}  // namespace


IconLabelBubbleView::IconLabelBubbleView(const int background_images[],
                                         int contained_image,
                                         const gfx::Font& font,
                                         int font_y_offset,
                                         SkColor color,
                                         bool elide_in_middle)
    : background_painter_(new views::HorizontalPainter(background_images)),
      image_(new views::ImageView()),
      label_(new views::Label()),
      is_extension_icon_(false) {
  image_->SetImage(
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          contained_image));
  AddChildView(image_);

  label_->set_border(views::Border::CreateEmptyBorder(font_y_offset, 0, 0, 0));
  label_->SetFont(font);
  label_->SetAutoColorReadabilityEnabled(false);
  label_->SetEnabledColor(color);
  if (elide_in_middle)
    label_->SetElideBehavior(views::Label::ELIDE_IN_MIDDLE);
  AddChildView(label_);
}

IconLabelBubbleView::~IconLabelBubbleView() {
}

void IconLabelBubbleView::SetLabel(const string16& label) {
  label_->SetText(label);
}

void IconLabelBubbleView::SetImage(const gfx::ImageSkia& image_skia) {
  image_->SetImage(image_skia);
}

gfx::Size IconLabelBubbleView::GetPreferredSize() {
  // Height will be ignored by the LocationBarView.
  return GetSizeForLabelWidth(label_->GetPreferredSize().width());
}

void IconLabelBubbleView::Layout() {
  image_->SetBounds(GetBubbleOuterPadding() +
      (is_extension_icon_ ? LocationBarView::kIconInternalPadding : 0), 0,
      image_->GetPreferredSize().width(), height());
  const int pre_label_width = GetPreLabelWidth();
  label_->SetBounds(pre_label_width, 0,
                    width() - pre_label_width - GetBubbleOuterPadding(),
                    label_->GetPreferredSize().height());
}

gfx::Size IconLabelBubbleView::GetSizeForLabelWidth(int width) const {
  gfx::Size size(GetPreLabelWidth() + width + GetBubbleOuterPadding(), 0);
  size.SetToMax(background_painter_->GetMinimumSize());
  return size;
}

void IconLabelBubbleView::OnPaint(gfx::Canvas* canvas) {
  background_painter_->Paint(canvas, size());
}

int IconLabelBubbleView::GetPreLabelWidth() const {
  const int image_width = image_->GetPreferredSize().width();
  return GetBubbleOuterPadding() +
      (image_width ? (image_width + LocationBarView::GetItemPadding()) : 0);
}
