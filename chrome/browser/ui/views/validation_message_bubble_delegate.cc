// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/validation_message_bubble_delegate.h"

#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

// static
const int ValidationMessageBubbleDelegate::kWindowMinWidth = 64;
// static
const int ValidationMessageBubbleDelegate::kWindowMaxWidth = 256;
static const int kPadding = 0;
static const int kIconTextMargin = 8;
static const int kTextVerticalMargin = 4;

ValidationMessageBubbleDelegate::ValidationMessageBubbleDelegate(
    const gfx::Rect& anchor_in_screen,
    const string16& main_text,
    const string16& sub_text,
    Observer* observer)
    : observer_(observer), width_(0), height_(0) {
  set_use_focusless(true);
  set_arrow(views::BubbleBorder::TOP_LEFT);
  set_anchor_rect(anchor_in_screen);

  ResourceBundle& bundle = ResourceBundle::GetSharedInstance();
  views::ImageView* icon = new views::ImageView();
  icon->SetImage(*bundle.GetImageSkiaNamed(IDR_INPUT_ALERT));
  gfx::Size size = icon->GetPreferredSize();
  icon->SetBounds(kPadding, kPadding, size.width(), size.height());
  AddChildView(icon);

  views::Label* label = new views::Label(main_text);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetFont(bundle.GetFont(ResourceBundle::MediumFont));
  label->set_directionality_mode(views::Label::AUTO_DETECT_DIRECTIONALITY);
  int text_start_x = kPadding + size.width() + kIconTextMargin;
  int min_available = kWindowMinWidth - text_start_x - kPadding;
  int max_available = kWindowMaxWidth - text_start_x - kPadding;
  int label_width = label->GetPreferredSize().width();
  label->SetMultiLine(true);
  AddChildView(label);

  views::Label* sub_label = NULL;
  if (!sub_text.empty()) {
    sub_label = new views::Label(sub_text);
    sub_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    sub_label->set_directionality_mode(
        views::Label::AUTO_DETECT_DIRECTIONALITY);
    label_width = std::max(label_width, sub_label->GetPreferredSize().width());
    sub_label->SetMultiLine(true);
    AddChildView(sub_label);
  }

  if (label_width < min_available)
    label_width = min_available;
  else if (label_width > max_available)
    label_width = max_available;
  label->SetBounds(text_start_x, kPadding,
                   label_width, label->GetHeightForWidth(label_width));
  int content_bottom = kPadding + label->height();

  if (sub_label) {
    sub_label->SetBounds(text_start_x,
                         content_bottom + kTextVerticalMargin,
                         label_width,
                         sub_label->GetHeightForWidth(label_width));
    content_bottom += kTextVerticalMargin + sub_label->height();
  }

  width_ = text_start_x + label_width + kPadding;
  height_ = content_bottom + kPadding;
}

ValidationMessageBubbleDelegate::~ValidationMessageBubbleDelegate() {}

void ValidationMessageBubbleDelegate::Close() {
  GetWidget()->Close();
  observer_ = NULL;
}

void ValidationMessageBubbleDelegate::SetPositionRelativeToAnchor(
    const gfx::Rect& anchor_in_screen) {
  set_anchor_rect(anchor_in_screen);
  SizeToContents();
}

gfx::Size ValidationMessageBubbleDelegate::GetPreferredSize() {
  return gfx::Size(width_, height_);
}

void ValidationMessageBubbleDelegate::DeleteDelegate() {
  delete this;
}

void ValidationMessageBubbleDelegate::WindowClosing() {
  if (observer_ != NULL)
    observer_->WindowClosing();
}
