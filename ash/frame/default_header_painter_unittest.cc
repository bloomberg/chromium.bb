// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame/default_header_painter.h"

#include "ash/frame/caption_buttons/frame_caption_button_container_view.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_state.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/font_list.h"
#include "ui/views/test/test_views.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

using ash::HeaderPainter;
using views::NonClientFrameView;
using views::Widget;

namespace ash {

class DefaultHeaderPainterTest : public ash::test::AshTestBase {
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
TEST_F(DefaultHeaderPainterTest, TitleIconAlignment) {
  scoped_ptr<Widget> w(CreateTestWidget());
  ash::FrameCaptionButtonContainerView container(w.get(),
  ash::FrameCaptionButtonContainerView::MINIMIZE_ALLOWED);
  views::StaticSizedView window_icon(gfx::Size(16, 16));
  window_icon.SetBounds(0, 0, 16, 16);
  w->SetBounds(gfx::Rect(0, 0, 500, 500));
  w->Show();

  DefaultHeaderPainter painter;
  painter.Init(w.get(),
               w->non_client_view()->frame_view(),
               &container);
  painter.UpdateLeftHeaderView(&window_icon);
  painter.LayoutHeader();
  gfx::Rect title_bounds = painter.GetTitleBounds();
  EXPECT_EQ(window_icon.bounds().CenterPoint().y(),
            title_bounds.CenterPoint().y());
}

}  // namespace ash
