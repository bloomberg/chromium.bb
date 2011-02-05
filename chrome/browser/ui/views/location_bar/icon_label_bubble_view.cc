// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"

#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"

// Amount of padding at the edges of the bubble.
static const int kBubbleOuterPadding =
    LocationBarView::kEdgeItemPadding - LocationBarView::kBubblePadding;

// Amount of padding after the label.
static const int kLabelPadding = 5;

IconLabelBubbleView::IconLabelBubbleView(const int background_images[],
                                         int contained_image,
                                         const SkColor& color)
    : background_painter_(background_images),
      item_padding_(LocationBarView::kItemPadding) {
  image_ = new views::ImageView();
  AddChildView(image_);
  image_->SetImage(
      ResourceBundle::GetSharedInstance().GetBitmapNamed(contained_image));
  label_ = new views::Label();
  AddChildView(label_);
  label_->SetColor(color);
}

IconLabelBubbleView::~IconLabelBubbleView() {
}

void IconLabelBubbleView::SetFont(const gfx::Font& font) {
  label_->SetFont(font);
}

void IconLabelBubbleView::SetLabel(const std::wstring& label) {
  label_->SetText(label);
}

void IconLabelBubbleView::SetImage(const SkBitmap& bitmap) {
  image_->SetImage(bitmap);
}

void IconLabelBubbleView::Paint(gfx::Canvas* canvas) {
  background_painter_.Paint(width(), height(), canvas);
}

gfx::Size IconLabelBubbleView::GetPreferredSize() {
  gfx::Size size(GetNonLabelSize());
  size.Enlarge(label_->GetPreferredSize().width(), 0);
  return size;
}

void IconLabelBubbleView::Layout() {
  image_->SetBounds(kBubbleOuterPadding, 0, image_->GetPreferredSize().width(),
                    height());
  const int label_height = label_->GetPreferredSize().height();
  label_->SetBounds(image_->x() + image_->width() +
      item_padding_ , (height() - label_height) / 2,
      width() - GetNonLabelWidth(), label_height);
}

void IconLabelBubbleView::SetElideInMiddle(bool elide_in_middle) {
  label_->SetElideInMiddle(elide_in_middle);
}

gfx::Size IconLabelBubbleView::GetNonLabelSize() {
  return gfx::Size(GetNonLabelWidth(), background_painter_.height());
}

int IconLabelBubbleView::GetNonLabelWidth() {
  return kBubbleOuterPadding + image_->GetPreferredSize().width() +
      item_padding_ + kBubbleOuterPadding;
}
