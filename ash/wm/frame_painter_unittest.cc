// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/frame_painter.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/scoped_ptr.h"
#include "grit/ui_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/root_window.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/widget/widget.h"

using views::Widget;
using views::ImageButton;

namespace {

aura::Window* GetDefaultContainer() {
  return ash::Shell::GetInstance()->GetContainer(
      ash::internal::kShellWindowId_DefaultContainer);
}

// Creates a test widget that owns its native widget.
Widget* CreateTestWidget() {
  Widget* widget = new Widget;
  Widget::InitParams params;
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = GetDefaultContainer();
  params.child = true;
  widget->Init(params);
  return widget;
}

Widget* CreateAlwaysOnTopWidget() {
  Widget* widget = new Widget;
  Widget::InitParams params;
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = GetDefaultContainer();
  params.child = true;
  params.keep_on_top = true;
  widget->Init(params);
  return widget;
}

}  // namespace

namespace ash {

typedef ash::test::AshTestBase FramePainterTest;

TEST_F(FramePainterTest, Basics) {
  // We start with a null instances pointer, as we don't initialize until the
  // first FramePainter is created.
  EXPECT_FALSE(FramePainter::instances_);

  // Creating a painter bumps the instance count.
  {
    FramePainter painter;
    EXPECT_TRUE(FramePainter::instances_);
    EXPECT_EQ(1u, FramePainter::instances_->size());
  }
  // Destroying that painter leaves a valid pointer but no instances.
  EXPECT_TRUE(FramePainter::instances_);
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
  SkBitmap custom_overlay;
  EXPECT_EQ(255,
            p1.GetHeaderOpacity(FramePainter::ACTIVE,
                                IDR_AURA_WINDOW_HEADER_BASE_ACTIVE,
                                &custom_overlay));
}

}  // namespace ash
