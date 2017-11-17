// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame/default_frame_header.h"

#include <memory>

#include "ash/frame/caption_buttons/frame_back_button.h"
#include "ash/frame/caption_buttons/frame_caption_button_container_view.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ui/views/test/test_views.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

using ash::FrameHeader;
using views::NonClientFrameView;
using views::Widget;

namespace ash {

using DefaultFrameHeaderTest = AshTestBase;

// Ensure the title text is vertically aligned with the window icon.
TEST_F(DefaultFrameHeaderTest, TitleIconAlignment) {
  std::unique_ptr<Widget> w = CreateTestWidget(
      nullptr, kShellWindowId_DefaultContainer, gfx::Rect(1, 2, 3, 4));
  ash::FrameCaptionButtonContainerView container(w.get());
  views::StaticSizedView window_icon(gfx::Size(16, 16));
  window_icon.SetBounds(0, 0, 16, 16);
  w->SetBounds(gfx::Rect(0, 0, 500, 500));
  w->Show();

  DefaultFrameHeader frame_header;
  frame_header.Init(w.get(), w->non_client_view()->frame_view(), &container,
                    nullptr);
  frame_header.UpdateLeftHeaderView(&window_icon);
  frame_header.LayoutHeader();
  gfx::Rect title_bounds = frame_header.GetTitleBounds();
  EXPECT_EQ(window_icon.bounds().CenterPoint().y(),
            title_bounds.CenterPoint().y());
}

TEST_F(DefaultFrameHeaderTest, BackButtonAlignment) {
  std::unique_ptr<Widget> w = CreateTestWidget(
      nullptr, kShellWindowId_DefaultContainer, gfx::Rect(1, 2, 3, 4));
  ash::FrameCaptionButtonContainerView container(w.get());
  ash::FrameBackButton back;

  DefaultFrameHeader frame_header;
  frame_header.Init(w.get(), w->non_client_view()->frame_view(), &container,
                    nullptr);
  frame_header.UpdateBackButton(&back);
  frame_header.LayoutHeader();
  gfx::Rect title_bounds = frame_header.GetTitleBounds();
  // The back button should be positioned at the left edge, and
  // vertically centered.
  EXPECT_EQ(back.bounds().CenterPoint().y(), title_bounds.CenterPoint().y());
  EXPECT_EQ(0, back.bounds().x());
}

// Ensure the light icons are used when appropriate.
TEST_F(DefaultFrameHeaderTest, LightIcons) {
  std::unique_ptr<Widget> w = CreateTestWidget(
      nullptr, kShellWindowId_DefaultContainer, gfx::Rect(1, 2, 3, 4));
  ash::FrameCaptionButtonContainerView container(w.get());
  views::StaticSizedView window_icon(gfx::Size(16, 16));
  window_icon.SetBounds(0, 0, 16, 16);
  w->SetBounds(gfx::Rect(0, 0, 500, 500));
  w->Show();

  DefaultFrameHeader frame_header;
  frame_header.Init(w.get(), w->non_client_view()->frame_view(), &container,
                    nullptr);

  // Check by default light icons are not used.
  frame_header.mode_ = FrameHeader::MODE_ACTIVE;
  EXPECT_FALSE(frame_header.ShouldUseLightImages());
  frame_header.mode_ = FrameHeader::MODE_INACTIVE;
  EXPECT_FALSE(frame_header.ShouldUseLightImages());

  // Check that setting dark colors should use light icons.
  frame_header.SetFrameColors(SkColorSetRGB(0, 0, 0), SkColorSetRGB(0, 0, 0));
  frame_header.mode_ = FrameHeader::MODE_ACTIVE;
  EXPECT_TRUE(frame_header.ShouldUseLightImages());
  frame_header.mode_ = FrameHeader::MODE_INACTIVE;
  EXPECT_TRUE(frame_header.ShouldUseLightImages());

  // Check that inactive and active colors are used properly.
  frame_header.SetFrameColors(SkColorSetRGB(0, 0, 0),
                              SkColorSetRGB(255, 255, 255));
  frame_header.mode_ = FrameHeader::MODE_ACTIVE;
  EXPECT_TRUE(frame_header.ShouldUseLightImages());
  frame_header.mode_ = FrameHeader::MODE_INACTIVE;
  EXPECT_FALSE(frame_header.ShouldUseLightImages());

  // Check not so light or dark colors.
  frame_header.SetFrameColors(SkColorSetRGB(70, 70, 70),
                              SkColorSetRGB(200, 200, 200));
  frame_header.mode_ = FrameHeader::MODE_ACTIVE;
  EXPECT_TRUE(frame_header.ShouldUseLightImages());
  frame_header.mode_ = FrameHeader::MODE_INACTIVE;
  EXPECT_FALSE(frame_header.ShouldUseLightImages());
}

}  // namespace ash
