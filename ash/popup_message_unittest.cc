// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/popup_message.h"

#include "ash/test/ash_test_base.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

namespace ash {

typedef test::AshTestBase PopupMessageTest;

// Verifies the layout of the popup, especially it does not crop the caption and
// message text. See http://crbug.com/468494.
TEST_F(PopupMessageTest, Layout) {
  views::Widget* widget =
      views::Widget::CreateWindowWithBounds(nullptr, gfx::Rect(0, 0, 100, 100));
  PopupMessage message(base::ASCIIToUTF16("caption text"),
                       base::ASCIIToUTF16(
                           "Message text, which will be usually longer than "
                           "the caption, so that it's wrapped at some width"),
                       PopupMessage::ICON_WARNING,
                       widget->GetContentsView() /* anchor */,
                       views::BubbleBorder::TOP_LEFT, gfx::Size(), 10);

  views::View* contents_view = message.widget_->GetContentsView();
  views::View* caption_label =
      contents_view->GetViewByID(PopupMessage::kCaptionLabelID);
  views::View* message_label =
      contents_view->GetViewByID(PopupMessage::kMessageLabelID);
  ASSERT_TRUE(caption_label);
  ASSERT_TRUE(message_label);

  // The bubble should have enough heights to show both of the labels.
  EXPECT_GE(contents_view->height(),
            caption_label->height() + message_label->height());

  // The labels are not cropped -- the assigned height has enough height to show
  // the full text.
  EXPECT_GE(caption_label->height(),
            caption_label->GetHeightForWidth(caption_label->width()));
  EXPECT_GE(message_label->height(),
            message_label->GetHeightForWidth(message_label->width()));

  message.Close();
  widget->Close();
}

}  // namespace ash
