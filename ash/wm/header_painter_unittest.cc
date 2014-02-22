// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/header_painter.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/caption_buttons/frame_caption_button_container_view.h"
#include "ash/wm/window_state.h"
#include "base/memory/scoped_ptr.h"
#include "grit/ash_resources.h"
#include "ui/gfx/font_list.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

using ash::HeaderPainter;
using views::NonClientFrameView;
using views::Widget;

namespace ash {

class HeaderPainterTest : public ash::test::AshTestBase {
 public:
  // Creates a test widget that owns its native widget.
  Widget* CreateTestWidget() {
    Widget* widget = new Widget;
    Widget::InitParams params;
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.context = CurrentContext();
    widget->Init(params);
    return widget;
  }
};

// Ensure the title text is vertically aligned with the window icon.
TEST_F(HeaderPainterTest, TitleIconAlignment) {
  scoped_ptr<Widget> w(CreateTestWidget());
  ash::FrameCaptionButtonContainerView container(w.get(),
      ash::FrameCaptionButtonContainerView::MINIMIZE_ALLOWED);
  views::View window_icon;
  window_icon.SetBounds(0, 0, 16, 16);
  w->SetBounds(gfx::Rect(0, 0, 500, 500));
  w->Show();

  gfx::FontList default_font_list;

  // 1) Non-browser windows.
  HeaderPainter non_browser_painter;
  non_browser_painter.Init(HeaderPainter::STYLE_OTHER,
      w.get(),
      w->non_client_view()->frame_view(),
      &window_icon,
      &container);
  non_browser_painter.LayoutHeader();
  gfx::Rect non_browser_header_title_bounds =
      non_browser_painter.GetTitleBounds(default_font_list);
  EXPECT_EQ(window_icon.bounds().CenterPoint().y(),
            non_browser_header_title_bounds.CenterPoint().y());

  // 2) Non-maximized browser windows.
  HeaderPainter browser_painter;
  browser_painter.Init(HeaderPainter::STYLE_BROWSER,
      w.get(),
      w->non_client_view()->frame_view(),
      &window_icon,
      &container);
  browser_painter.LayoutHeader();
  gfx::Rect browser_header_title_bounds =
      browser_painter.GetTitleBounds(default_font_list);
  EXPECT_EQ(window_icon.bounds().CenterPoint().y(),
            browser_header_title_bounds.CenterPoint().y());

  // 3) Maximized browser windows.
  w->Maximize();
  browser_painter.LayoutHeader();
  gfx::Rect maximized_browser_header_title_bounds =
      browser_painter.GetTitleBounds(default_font_list);
  EXPECT_EQ(window_icon.bounds().CenterPoint().y(),
            maximized_browser_header_title_bounds.CenterPoint().y());
}

}  // namespace ash
