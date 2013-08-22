// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/frame_caption_button_container_view.h"

#include "ash/ash_switches.h"
#include "base/command_line.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"

namespace ash {

typedef testing::Test FrameCaptionButtonContainerViewTest;

TEST_F(FrameCaptionButtonContainerViewTest, Sanity) {
  // 1) Test the layout and the caption button visibility in the default case.
  // Both the size button and the close button should be visible.
  FrameCaptionButtonContainerView c1(NULL, NULL);
  c1.Layout();
  FrameCaptionButtonContainerView::TestApi t1(&c1);
  views::ImageButton* size_button = t1.size_button();
  views::ImageButton* close_button = t1.close_button();
  EXPECT_TRUE(size_button->visible());
  EXPECT_TRUE(close_button->visible());

  // The size button should be left of the close button. (in non-RTL)
  EXPECT_LT(size_button->x(), close_button->x());

  // The container's bounds should be flush with the caption buttons.
  EXPECT_EQ(0, size_button->x());
  EXPECT_EQ(c1.GetPreferredSize().width(), close_button->bounds().right());
  EXPECT_EQ(c1.GetPreferredSize().height(), close_button->bounds().height());

  // 2) Test the layout and the caption button visibility when the
  // "force-maximize-mode" experiment is turned on. Only the close button
  // should be visible.
  CommandLine::ForCurrentProcess()->AppendSwitch(switches::kForcedMaximizeMode);

  FrameCaptionButtonContainerView c2(NULL, NULL);
  c2.Layout();
  FrameCaptionButtonContainerView::TestApi t2(&c2);
  size_button = t2.size_button();
  close_button = t2.close_button();

  EXPECT_FALSE(size_button->visible());
  EXPECT_TRUE(close_button->visible());
  EXPECT_EQ(c2.GetPreferredSize().width(), close_button->width());
  EXPECT_EQ(c2.GetPreferredSize().height(), close_button->height());
}

// Test the layout when a border is set on the container.
TEST_F(FrameCaptionButtonContainerViewTest, Border) {
  const int kTopInset = 1;
  const int kLeftInset = 2;
  const int kBottomInset = 3;
  const int kRightInset = 4;

  FrameCaptionButtonContainerView c(NULL, NULL);
  c.set_border(views::Border::CreateEmptyBorder(
      kTopInset, kLeftInset, kBottomInset, kRightInset));
  c.Layout();
  FrameCaptionButtonContainerView::TestApi t(&c);

  EXPECT_EQ(kLeftInset, t.size_button()->x());
  EXPECT_EQ(kTopInset, t.close_button()->y());
  EXPECT_EQ(c.GetPreferredSize().height(),
            t.close_button()->bounds().bottom() + kBottomInset);
  EXPECT_EQ(c.GetPreferredSize().width(),
            t.close_button()->bounds().right() + kRightInset);
}

}  // namespace ash
