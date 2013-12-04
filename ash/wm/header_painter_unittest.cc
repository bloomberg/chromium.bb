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
#include "ui/gfx/font.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

using ash::HeaderPainter;
using views::NonClientFrameView;
using views::Widget;

namespace {

// Modifies the values of kInactiveWindowOpacity, kActiveWindowOpacity, and
// kSoloWindowOpacity for the lifetime of the class. This is useful so that
// the constants each have different values.
class ScopedOpacityConstantModifier {
 public:
  ScopedOpacityConstantModifier()
      : initial_active_window_opacity_(
            ash::HeaderPainter::kActiveWindowOpacity),
        initial_inactive_window_opacity_(
            ash::HeaderPainter::kInactiveWindowOpacity),
        initial_solo_window_opacity_(ash::HeaderPainter::kSoloWindowOpacity) {
    ash::HeaderPainter::kActiveWindowOpacity = 100;
    ash::HeaderPainter::kInactiveWindowOpacity = 120;
    ash::HeaderPainter::kSoloWindowOpacity = 140;
  }
  ~ScopedOpacityConstantModifier() {
    ash::HeaderPainter::kActiveWindowOpacity = initial_active_window_opacity_;
    ash::HeaderPainter::kInactiveWindowOpacity =
        initial_inactive_window_opacity_;
    ash::HeaderPainter::kSoloWindowOpacity = initial_solo_window_opacity_;
  }

 private:
  int initial_active_window_opacity_;
  int initial_inactive_window_opacity_;
  int initial_solo_window_opacity_;

  DISALLOW_COPY_AND_ASSIGN(ScopedOpacityConstantModifier);
};

// Creates a new HeaderPainter with empty buttons. Caller owns the memory.
HeaderPainter* CreateTestPainter(Widget* widget) {
  HeaderPainter* painter = new HeaderPainter();
  NonClientFrameView* frame_view = widget->non_client_view()->frame_view();
  ash::FrameCaptionButtonContainerView* container =
      new ash::FrameCaptionButtonContainerView(
          widget,
          ash::FrameCaptionButtonContainerView::MINIMIZE_ALLOWED);
  // Add the container to the widget's non-client frame view so that it will be
  // deleted when the widget is destroyed.
  frame_view->AddChildView(container);
  painter->Init(widget, frame_view, NULL, container);
  return painter;
}

}  // namespace

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

TEST_F(HeaderPainterTest, GetHeaderOpacity) {
  // Create a widget and a painter for it.
  scoped_ptr<Widget> w1(CreateTestWidget());
  scoped_ptr<HeaderPainter> p1(CreateTestPainter(w1.get()));
  w1->Show();

  // Modify the values of the opacity constants so that they each have a
  // different value.
  ScopedOpacityConstantModifier opacity_constant_modifier;

  // Solo active window has solo window opacity.
  EXPECT_EQ(HeaderPainter::kSoloWindowOpacity,
            p1->GetHeaderOpacity(HeaderPainter::ACTIVE,
                                 IDR_AURA_WINDOW_HEADER_BASE_ACTIVE,
                                 0));

  // Create a second widget and painter.
  scoped_ptr<Widget> w2(CreateTestWidget());
  scoped_ptr<HeaderPainter> p2(CreateTestPainter(w2.get()));
  w2->Show();

  // Active window has active window opacity.
  EXPECT_EQ(HeaderPainter::kActiveWindowOpacity,
            p2->GetHeaderOpacity(HeaderPainter::ACTIVE,
                                 IDR_AURA_WINDOW_HEADER_BASE_ACTIVE,
                                 0));

  // Inactive window has inactive window opacity.
  EXPECT_EQ(HeaderPainter::kInactiveWindowOpacity,
            p2->GetHeaderOpacity(HeaderPainter::INACTIVE,
                                 IDR_AURA_WINDOW_HEADER_BASE_INACTIVE,
                                 0));

  // Regular maximized windows are fully opaque.
  wm::GetWindowState(w1->GetNativeWindow())->Maximize();
  EXPECT_EQ(255,
            p1->GetHeaderOpacity(HeaderPainter::ACTIVE,
                                 IDR_AURA_WINDOW_HEADER_BASE_ACTIVE,
                                 0));
}

// Ensure the title text is vertically aligned with the window icon.
TEST_F(HeaderPainterTest, TitleIconAlignment) {
  scoped_ptr<Widget> w(CreateTestWidget());
  HeaderPainter p;
  ash::FrameCaptionButtonContainerView container(w.get(),
      ash::FrameCaptionButtonContainerView::MINIMIZE_ALLOWED);
  views::View window_icon;
  window_icon.SetBounds(0, 0, 16, 16);
  p.Init(w.get(),
         w->non_client_view()->frame_view(),
         &window_icon,
         &container);
  w->SetBounds(gfx::Rect(0, 0, 500, 500));
  w->Show();

  // Title and icon are aligned when shorter_header is false.
  p.LayoutHeader(false);
  gfx::Font default_font;
  gfx::Rect large_header_title_bounds = p.GetTitleBounds(default_font);
  EXPECT_EQ(window_icon.bounds().CenterPoint().y(),
            large_header_title_bounds.CenterPoint().y());

  // Title and icon are aligned when shorter_header is true.
  p.LayoutHeader(true);
  gfx::Rect short_header_title_bounds = p.GetTitleBounds(default_font);
  EXPECT_EQ(window_icon.bounds().CenterPoint().y(),
            short_header_title_bounds.CenterPoint().y());
}

}  // namespace ash
