// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/ui/desktop_ui/find_bar_view.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "mandoline/ui/desktop_ui/find_bar_delegate.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"

namespace mandoline {

FindBarView::FindBarView(FindBarDelegate* delegate)
    : delegate_(delegate),
      layout_(new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 5)),
      text_field_(new views::Textfield),
      match_count_label_(new views::Label),
      next_button_(new views::LabelButton(this, base::ASCIIToUTF16("Next"))),
      prev_button_(new views::LabelButton(this, base::ASCIIToUTF16("Prev"))),
      close_button_(new views::LabelButton(this, base::ASCIIToUTF16("Close"))) {
  SetLayoutManager(layout_);

  text_field_->set_controller(this);

  AddChildView(text_field_);
  AddChildView(match_count_label_);
  AddChildView(next_button_);
  AddChildView(prev_button_);
  AddChildView(close_button_);

  layout_->SetDefaultFlex(0);
  layout_->SetFlexForView(text_field_, 1);

  SetVisible(false);
  SetMatchLabel(0, 0);
}

FindBarView::~FindBarView() {}

void FindBarView::Show() {
  SetVisible(true);
  text_field_->RequestFocus();
}

void FindBarView::Hide() {
  last_find_string_.clear();
  SetVisible(false);
}

void FindBarView::SetMatchLabel(int result, int total) {
  std::string str = base::StringPrintf("%d of %d", result, total);
  match_count_label_->SetVisible(true);
  match_count_label_->SetText(base::UTF8ToUTF16(str));
  Layout();
}

void FindBarView::ContentsChanged(views::Textfield* sender,
                                  const base::string16& new_contents) {
  std::string contents = base::UTF16ToUTF8(new_contents);
  last_find_string_ = contents;
  delegate_->OnDoFind(contents, true);
}

void FindBarView::ButtonPressed(views::Button* sender, const ui::Event& event) {
  if (sender == next_button_) {
    delegate_->OnDoFind(last_find_string_, true);
  } else if (sender == prev_button_) {
    delegate_->OnDoFind(last_find_string_, false);
  } else if (sender == close_button_) {
    last_find_string_.clear();
    delegate_->OnHideFindBar();
  } else {
    NOTREACHED();
  }
}

}  // namespace mandoline
