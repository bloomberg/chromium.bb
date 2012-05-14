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

// Amount of padding at the edges of the bubble.
static const int kBubbleOuterPadding = LocationBarView::kEdgeItemPadding -
    LocationBarView::kBubbleHorizontalPadding;

// Amount of padding after the label.
static const int kLabelPadding = 5;

IconLabelBubbleView::IconLabelBubbleView(const int background_images[],
                                         int contained_image,
                                         SkColor color)
    : background_painter_(background_images),
      is_extension_icon_(false) {
  image_ = new views::ImageView();
  image_->SetImage(
      ui::ResourceBundle::GetSharedInstance().GetBitmapNamed(contained_image));
  AddChildView(image_);

  label_ = new views::Label();
  label_->SetAutoColorReadabilityEnabled(false);
  label_->SetEnabledColor(color);
  AddChildView(label_);
}

IconLabelBubbleView::~IconLabelBubbleView() {
}

void IconLabelBubbleView::SetFont(const gfx::Font& font) {
  label_->SetFont(font);
}

void IconLabelBubbleView::SetLabel(const string16& label) {
  label_->SetText(label);
}

void IconLabelBubbleView::SetImage(const SkBitmap& bitmap) {
  image_->SetImage(bitmap);
}

void IconLabelBubbleView::OnPaint(gfx::Canvas* canvas) {
  background_painter_.Paint(canvas, size());
}

gfx::Size IconLabelBubbleView::GetPreferredSize() {
  gfx::Size size(GetNonLabelSize());
  size.Enlarge(label_->GetPreferredSize().width(), 0);
  return size;
}

void IconLabelBubbleView::Layout() {
  image_->SetBounds(kBubbleOuterPadding +
      (is_extension_icon_ ? LocationBarView::kIconInternalPadding : 0), 0,
      image_->GetPreferredSize().width(), height());
  const int label_height = label_->GetPreferredSize().height();
  label_->SetBounds(GetPreLabelWidth(), (height() - label_height) / 2,
                    width() - GetNonLabelWidth(), label_height);
}

void IconLabelBubbleView::SetElideInMiddle(bool elide_in_middle) {
  label_->SetElideInMiddle(elide_in_middle);
}

gfx::Size IconLabelBubbleView::GetNonLabelSize() const {
  return gfx::Size(GetNonLabelWidth(), background_painter_.height());
}

int IconLabelBubbleView::GetPreLabelWidth() const {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return kBubbleOuterPadding + rb.GetBitmapNamed(IDR_OMNIBOX_SEARCH)->width() +
      LocationBarView::kItemPadding;
}

int IconLabelBubbleView::GetNonLabelWidth() const {
  return GetPreLabelWidth() + kBubbleOuterPadding;
}
