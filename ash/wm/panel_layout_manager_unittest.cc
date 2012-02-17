// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/panel_layout_manager.h"

#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

namespace {

views::Widget* CreatePanelWindow(const gfx::Rect& rect) {
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_PANEL);
  params.bounds = rect;
  views::Widget* widget = new views::Widget();
  widget->Init(params);
  widget->Show();
  return widget;
}

class PanelLayoutManagerTest : public ash::test::AshTestBase {
 public:
  PanelLayoutManagerTest() {}
  virtual ~PanelLayoutManagerTest() {}

  aura::Window* GetPanelContainer() {
    return Shell::GetInstance()->GetContainer(
        ash::internal::kShellWindowId_PanelContainer);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PanelLayoutManagerTest);
};

}  // namespace

// Tests that a created panel window is successfully added to the panel
// layout manager.
TEST_F(PanelLayoutManagerTest, AddOnePanel) {
  gfx::Rect bounds(1, 1, 200, 200);
  views::Widget* w1 = CreatePanelWindow(bounds);
  EXPECT_EQ(GetPanelContainer(), w1->GetNativeWindow()->parent());
}

// Tests that panels are ordered right-to-left.
TEST_F(PanelLayoutManagerTest, PanelOrderRightToLeft) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAuraPanelManager))
    return;
  gfx::Rect bounds(1, 1, 200, 200);
  views::Widget* w1 = CreatePanelWindow(bounds);
  views::Widget* w2 = CreatePanelWindow(bounds);
  EXPECT_LT(w2->GetWindowScreenBounds().x(), w1->GetWindowScreenBounds().x());

  views::Widget* w3 = CreatePanelWindow(bounds);
  EXPECT_LT(w3->GetWindowScreenBounds().x(), w2->GetWindowScreenBounds().x());
  EXPECT_LT(w2->GetWindowScreenBounds().x(), w1->GetWindowScreenBounds().x());
}

// Tests removing a panel.
TEST_F(PanelLayoutManagerTest, RemovePanel) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAuraPanelManager))
    return;

  gfx::Rect bounds(1, 1, 200, 200);
  views::Widget* w1 = CreatePanelWindow(bounds);
  views::Widget* w2 = CreatePanelWindow(bounds);
  views::Widget* w3 = CreatePanelWindow(bounds);

  gfx::Rect w3bounds = w3->GetWindowScreenBounds();

  GetPanelContainer()->RemoveChild(w2->GetNativeWindow());

  // Verify that w3 has moved.
  EXPECT_NE(w3->GetWindowScreenBounds(), w3bounds);
  // Verify that w3 is still left of w1.
  EXPECT_LT(w3->GetWindowScreenBounds().x(), w1->GetWindowScreenBounds().x());
}

}  // namespace ash
