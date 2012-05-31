// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/message_bubble.h"

#include <vector>

#include "base/logging.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "grit/generated_resources.h"
#include "grit/ui_resources_standard.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"

namespace {

const int kBorderSize = 4;
const int kMaxLabelWidth = 250;

}  // namespace

namespace chromeos {

MessageBubble::MessageBubble(views::View* anchor_view,
                             views::BubbleBorder::ArrowLocation arrow_location,
                             SkBitmap* image,
                             const string16& text,
                             const std::vector<string16>& links)
    : BubbleDelegateView(anchor_view, arrow_location),
      image_(image),
      text_(text),
      close_button_(NULL),
      link_listener_(NULL) {
  set_use_focusless(true);
  for (size_t i = 0; i < links.size(); ++i)
    help_links_.push_back(new views::Link(links[i]));
}

MessageBubble::~MessageBubble() {
}

void MessageBubble::Init() {
  using views::GridLayout;

  GridLayout* layout = new GridLayout(this);
  SetLayoutManager(layout);
  views::ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::LEADING, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kBorderSize);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kBorderSize);
  column_set->AddColumn(GridLayout::TRAILING, GridLayout::LEADING, 0,
                        GridLayout::USE_PREF, 0, 0);
  if (!help_links_.empty()) {
    column_set = layout->AddColumnSet(1);
    column_set->AddPaddingColumn(0, kBorderSize + image_->width());
    column_set->AddColumn(GridLayout::LEADING, GridLayout::LEADING, 1,
                          GridLayout::USE_PREF, 0, 0);
  }

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  layout->StartRow(0, 0);
  views::ImageView* icon = new views::ImageView();
  icon->SetImage(*image_);
  layout->AddView(icon);

  views::Label* label = new views::Label(text_);
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  label->SizeToFit(kMaxLabelWidth);
  layout->AddView(label);

  close_button_ = new views::ImageButton(this);
  close_button_->SetImage(views::CustomButton::BS_NORMAL,
      rb.GetImageSkiaNamed(IDR_CLOSE_BAR));
  close_button_->SetImage(views::CustomButton::BS_HOT,
      rb.GetImageSkiaNamed(IDR_CLOSE_BAR_H));
  close_button_->SetImage(views::CustomButton::BS_PUSHED,
      rb.GetImageSkiaNamed(IDR_CLOSE_BAR_P));
  layout->AddView(close_button_);

  for (size_t i = 0; i < help_links_.size(); ++i) {
    layout->StartRowWithPadding(0, 1, 0, kBorderSize);
    views::Link* help_link = help_links_[i];
    help_link->set_listener(this);
    help_link->SetEnabledColor(login::kLinkColor);
    help_link->SetPressedColor(login::kLinkColor);
    layout->AddView(help_link);
  }
}

void MessageBubble::ButtonPressed(views::Button* sender,
                                  const views::Event& event) {
  if (sender == close_button_) {
    GetWidget()->Close();
  } else {
    NOTREACHED() << "Unknown view";
  }
}

void MessageBubble::LinkClicked(views::Link* source, int event_flags) {
  if (!link_listener_)
    return;
  for (size_t i = 0; i < help_links_.size(); ++i) {
    if (source == help_links_[i]) {
      link_listener_->OnLinkActivated(i);
      return;
    }
  }
  NOTREACHED() << "Unknown view";
}

}  // namespace chromeos
