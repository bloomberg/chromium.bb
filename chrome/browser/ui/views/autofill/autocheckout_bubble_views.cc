// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autocheckout_bubble_views.h"

#include "base/callback.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/autocheckout_bubble.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

namespace {

// TODO(ahutter): I18n these strings. See http://crbug.com/172116.
const char kPromptText[] = "Complete this form without typing?";
const char kAcceptText[] = "Auto-Checkout";
const char kCancelText[] = "No, Thanks";

}  // namespace

namespace autofill {

AutocheckoutBubbleViews::AutocheckoutBubbleViews(const gfx::RectF& bounding_box,
                                                 const base::Closure& callback)
  : views::BubbleDelegateView(NULL, views::BubbleBorder::TOP_LEFT),
    ok_button_(NULL),
    cancel_button_(NULL),
    bounding_box_(bounding_box),
    callback_(callback) {
  // TODO(ahutter): Add UMA stats here when they are finalized. See
  // http://crbug.com/172118.
}

AutocheckoutBubbleViews::~AutocheckoutBubbleViews() {
  // TODO(ahutter): Add UMA stats here when they are finalized. See
  // http://crbug.com/172118.
}

void AutocheckoutBubbleViews::Init() {
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);

  // Add the message label to the first row.
  views::ColumnSet* cs = layout->AddColumnSet(1);
  views::Label* message_label =
      new views::Label(ASCIIToUTF16(kPromptText));

  // Maximum width for the message field in pixels. The message text will be
  // wrapped when its width is wider than this.
  const int kMaxMessageWidth = 400;

  int message_width =
      std::min(kMaxMessageWidth, message_label->GetPreferredSize().width());
  cs->AddColumn(views::GridLayout::LEADING,
                views::GridLayout::CENTER,
                0,
                views::GridLayout::FIXED,
                message_width,
                false);
  message_label->SetBounds(0, 0, message_width, 0);
  message_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  message_label->SetMultiLine(true);
  layout->StartRow(0, 1);
  layout->AddView(message_label);

  // Padding between message and buttons.
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  cs = layout->AddColumnSet(2);
  cs->AddPaddingColumn(1, views::kRelatedControlHorizontalSpacing);
  cs->AddColumn(views::GridLayout::CENTER,
                views::GridLayout::CENTER,
                0,
                views::GridLayout::USE_PREF,
                0,
                0);
  cs->AddPaddingColumn(0, views::kRelatedButtonHSpacing);
  cs->AddColumn(views::GridLayout::CENTER,
                views::GridLayout::CENTER,
                0,
                views::GridLayout::USE_PREF,
                0,
                0);
  layout->StartRow(0, 2);
  ok_button_ = new views::LabelButton(this, ASCIIToUTF16(kAcceptText));
  ok_button_->SetStyle(views::Button::STYLE_NATIVE_TEXTBUTTON);
  layout->AddView(ok_button_);
  cancel_button_ = new views::LabelButton(this, ASCIIToUTF16(kCancelText));
  cancel_button_->SetStyle(views::Button::STYLE_NATIVE_TEXTBUTTON);
  layout->AddView(cancel_button_);
}

gfx::Rect AutocheckoutBubbleViews::GetAnchorRect() {
  return gfx::ToEnclosingRect(bounding_box_);
}

void AutocheckoutBubbleViews::ButtonPressed(views::Button* sender,
                                            const ui::Event& event)  {
  if (sender == ok_button_) {
    // TODO(ahutter): Add UMA stats here when they are finalized. See
    // http://crbug.com/172118.
    callback_.Run();
  } else if (sender == cancel_button_) {
    // TODO(ahutter): Add UMA stats here when they are finalized. See
    // http://crbug.com/172118.
  }
  GetWidget()->Close();
}

void ShowAutocheckoutBubble(const gfx::RectF& anchor,
                            const gfx::NativeView& native_view,
                            const base::Closure& callback) {
  AutocheckoutBubbleViews* delegate = new AutocheckoutBubbleViews(anchor,
                                                                  callback);
  delegate->set_parent_window(native_view);
  views::BubbleDelegateView::CreateBubble(delegate);
  delegate->SetAlignment(views::BubbleBorder::ALIGN_EDGE_TO_ANCHOR_EDGE);
  delegate->StartFade(true);
}

}  // namespace autofill
