// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/frame_painter.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/property_util.h"
#include "base/memory/scoped_ptr.h"
#include "grit/ash_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/root_window.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/screen.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/non_client_view.h"

using views::Widget;
using views::ImageButton;

namespace {

class ResizableWidgetDelegate : public views::WidgetDelegate {
 public:
  ResizableWidgetDelegate(views::Widget* widget) {
    widget_ = widget;
  }

  virtual bool CanResize() const OVERRIDE { return true; }
  // Implementations of the widget class.
  virtual views::Widget* GetWidget() OVERRIDE { return widget_; }
  virtual const views::Widget* GetWidget() const OVERRIDE { return widget_; }

 private:
  views::Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(ResizableWidgetDelegate);
};

// Creates a test widget that owns its native widget.
Widget* CreateTestWidget() {
  Widget* widget = new Widget;
  Widget::InitParams params;
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget->Init(params);
  return widget;
}

Widget* CreateAlwaysOnTopWidget() {
  Widget* widget = new Widget;
  Widget::InitParams params;
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.keep_on_top = true;
  widget->Init(params);
  return widget;
}

Widget* CreateResizableWidget() {
  Widget* widget = new Widget;
  Widget::InitParams params;
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.keep_on_top = true;
  params.delegate = new ResizableWidgetDelegate(widget);
  params.type = Widget::InitParams::TYPE_WINDOW;
  widget->Init(params);
  return widget;
}

}  // namespace

namespace ash {

typedef ash::test::AshTestBase FramePainterTest;

TEST_F(FramePainterTest, Basics) {
  // Other tests might have created a FramePainter, so we cannot assert that
  // FramePainter::instances_ is NULL here.

  // Creating a painter bumps the instance count.
  scoped_ptr<FramePainter> painter(new FramePainter);
  ASSERT_TRUE(FramePainter::instances_);
  EXPECT_EQ(1u, FramePainter::instances_->size());

  // Destroying that painter leaves a valid pointer but no instances.
  painter.reset();
  ASSERT_TRUE(FramePainter::instances_);
  EXPECT_EQ(0u, FramePainter::instances_->size());
}

TEST_F(FramePainterTest, UseSoloWindowHeader) {
  // No windows, no solo window mode.
  EXPECT_FALSE(FramePainter::UseSoloWindowHeader());

  // Create a widget and a painter for it.
  scoped_ptr<Widget> w1(CreateTestWidget());
  FramePainter p1;
  ImageButton size1(NULL);
  ImageButton close1(NULL);
  p1.Init(w1.get(), NULL, &size1, &close1, FramePainter::SIZE_BUTTON_MAXIMIZES);
  w1->Show();

  // We only have one window, so it should use a solo header.
  EXPECT_TRUE(FramePainter::UseSoloWindowHeader());

  // Create a second widget and painter.
  scoped_ptr<Widget> w2(CreateTestWidget());
  FramePainter p2;
  ImageButton size2(NULL);
  ImageButton close2(NULL);
  p2.Init(w2.get(), NULL, &size2, &close2, FramePainter::SIZE_BUTTON_MAXIMIZES);
  w2->Show();

  // Now there are two windows, so we should not use solo headers.
  EXPECT_FALSE(FramePainter::UseSoloWindowHeader());

  // Hide one window.  Solo should be enabled.
  w2->Hide();
  EXPECT_TRUE(FramePainter::UseSoloWindowHeader());

  // Show that window.  Solo should be disabled.
  w2->Show();
  EXPECT_FALSE(FramePainter::UseSoloWindowHeader());

  // Minimize the window.  Solo should be enabled.
  w2->Minimize();
  EXPECT_TRUE(FramePainter::UseSoloWindowHeader());

  // Close the minimized window.
  w2.reset();
  EXPECT_TRUE(FramePainter::UseSoloWindowHeader());

  // Open an always-on-top widget (which lives in a different container).
  scoped_ptr<Widget> w3(CreateAlwaysOnTopWidget());
  FramePainter p3;
  ImageButton size3(NULL);
  ImageButton close3(NULL);
  p3.Init(w3.get(), NULL, &size3, &close3, FramePainter::SIZE_BUTTON_MAXIMIZES);
  w3->Show();
  EXPECT_FALSE(FramePainter::UseSoloWindowHeader());

  // Close the always-on-top widget.
  w3.reset();
  EXPECT_TRUE(FramePainter::UseSoloWindowHeader());

  // Close the first window.
  w1.reset();
  EXPECT_FALSE(FramePainter::UseSoloWindowHeader());
}

TEST_F(FramePainterTest, GetHeaderOpacity) {
  // Create a widget and a painter for it.
  scoped_ptr<Widget> w1(CreateTestWidget());
  FramePainter p1;
  ImageButton size1(NULL);
  ImageButton close1(NULL);
  p1.Init(w1.get(), NULL, &size1, &close1, FramePainter::SIZE_BUTTON_MAXIMIZES);
  w1->Show();

  // Solo active window has solo window opacity.
  EXPECT_EQ(FramePainter::kSoloWindowOpacity,
            p1.GetHeaderOpacity(FramePainter::ACTIVE,
                                IDR_AURA_WINDOW_HEADER_BASE_ACTIVE,
                                NULL));

  // Create a second widget and painter.
  scoped_ptr<Widget> w2(CreateTestWidget());
  FramePainter p2;
  ImageButton size2(NULL);
  ImageButton close2(NULL);
  p2.Init(w2.get(), NULL, &size2, &close2, FramePainter::SIZE_BUTTON_MAXIMIZES);
  w2->Show();

  // Active window has active window opacity.
  EXPECT_EQ(FramePainter::kActiveWindowOpacity,
            p2.GetHeaderOpacity(FramePainter::ACTIVE,
                                IDR_AURA_WINDOW_HEADER_BASE_ACTIVE,
                                NULL));

  // Inactive window has inactive window opacity.
  EXPECT_EQ(FramePainter::kInactiveWindowOpacity,
            p2.GetHeaderOpacity(FramePainter::INACTIVE,
                                IDR_AURA_WINDOW_HEADER_BASE_INACTIVE,
                                NULL));

  // Custom overlay image is drawn completely opaque.
  gfx::ImageSkia custom_overlay;
  EXPECT_EQ(255,
            p1.GetHeaderOpacity(FramePainter::ACTIVE,
                                IDR_AURA_WINDOW_HEADER_BASE_ACTIVE,
                                &custom_overlay));
}

// Test the hit test function with windows which are "partially maximized".
TEST_F(FramePainterTest, HitTestSpecialMaximizedModes) {
  // Create a widget and a painter for it.
  scoped_ptr<Widget> w1(CreateResizableWidget());
  FramePainter p1;
  ImageButton size1(NULL);
  ImageButton close1(NULL);
  p1.Init(w1.get(), NULL, &size1, &close1, FramePainter::SIZE_BUTTON_MAXIMIZES);
  views::NonClientFrameView* frame = w1->non_client_view()->frame_view();
  w1->Show();
  gfx::Rect any_rect = gfx::Rect(0, 0, 100, 100);
  gfx::Rect screen = gfx::Screen::GetDisplayMatching(any_rect).work_area();
  w1->SetBounds(any_rect);
  EXPECT_EQ(HTTOPLEFT, p1.NonClientHitTest(frame, gfx::Point(0, 15)));
  w1->SetBounds(gfx::Rect(
      screen.x(), screen.y(), screen.width() / 2, screen.height()));
  // A hit without a set restore rect should produce a top left hit.
  EXPECT_EQ(HTTOPLEFT, p1.NonClientHitTest(frame, gfx::Point(0, 15)));
  ash::SetRestoreBoundsInScreen(w1->GetNativeWindow(), any_rect);
  // A hit into the corner should produce nowhere - not left.
  EXPECT_EQ(HTCAPTION, p1.NonClientHitTest(frame, gfx::Point(0, 15)));
  // A hit into the middle upper area should generate right - not top&right.
  EXPECT_EQ(HTRIGHT,
            p1.NonClientHitTest(frame, gfx::Point(screen.width() / 2, 15)));
  // A hit into the middle should generate right.
  EXPECT_EQ(HTRIGHT,
            p1.NonClientHitTest(frame, gfx::Point(screen.width() / 2,
                                                  screen.height() / 2)));
  // A hit into the middle lower area should generate right - not bottom&right.
  EXPECT_EQ(HTRIGHT,
            p1.NonClientHitTest(frame, gfx::Point(screen.width() / 2,
                                                  screen.height() - 1)));
}

}  // namespace ash
