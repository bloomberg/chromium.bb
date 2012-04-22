// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/panel_layout_manager.h"

#include "ash/ash_switches.h"
#include "ash/launcher/launcher.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_launcher_delegate.h"
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
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = rect;
  params.child = true;
  views::Widget* widget = new views::Widget();
  widget->Init(params);
  ash::test::TestLauncherDelegate* launcher_delegate =
      ash::test::TestLauncherDelegate::instance();
  CHECK(launcher_delegate);
  launcher_delegate->AddLauncherItem(widget->GetNativeWindow());
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

void IsPanelAboveLauncherIcon(views::Widget* panel) {
  Launcher* launcher = Shell::GetInstance()->launcher();
  aura::Window* window = panel->GetNativeWindow();
  gfx::Rect icon_bounds = launcher->GetScreenBoundsOfItemIconForWindow(window);
  ASSERT_FALSE(icon_bounds.IsEmpty());

  gfx::Rect window_bounds = panel->GetWindowScreenBounds();

  // 1-pixel tolerance--since we center panels over their icons, panels with odd
  // pixel widths won't be perfectly lined up with even pixel width launcher
  // icons.
  EXPECT_NEAR(
      window_bounds.CenterPoint().x(), icon_bounds.CenterPoint().x(), 1);
  EXPECT_EQ(window_bounds.bottom(), icon_bounds.y());
}

}  // namespace

// Tests that a created panel window is successfully added to the panel
// layout manager.
TEST_F(PanelLayoutManagerTest, AddOnePanel) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kAuraPanelManager))
    return;

  gfx::Rect bounds(0, 0, 201, 201);
  scoped_ptr<views::Widget> window(CreatePanelWindow(bounds));
  EXPECT_EQ(GetPanelContainer(), window->GetNativeWindow()->parent());
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(window.get()));
}

// Tests that panels are ordered right-to-left.
TEST_F(PanelLayoutManagerTest, PanelAboveLauncherIcons) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kAuraPanelManager))
    return;

  gfx::Rect bounds(0, 0, 201, 201);
  scoped_ptr<views::Widget> w1(CreatePanelWindow(bounds));
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w1.get()));
  scoped_ptr<views::Widget> w2(CreatePanelWindow(bounds));
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w1.get()));
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w2.get()));
  scoped_ptr<views::Widget> w3(CreatePanelWindow(bounds));
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w1.get()));
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w2.get()));
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w3.get()));
}

// Tests removing a panel.
TEST_F(PanelLayoutManagerTest, RemovePanel) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kAuraPanelManager))
    return;

  gfx::Rect bounds(0, 0, 201, 201);
  scoped_ptr<views::Widget> w1(CreatePanelWindow(bounds));
  scoped_ptr<views::Widget> w2(CreatePanelWindow(bounds));
  scoped_ptr<views::Widget> w3(CreatePanelWindow(bounds));

  GetPanelContainer()->RemoveChild(w2->GetNativeWindow());

  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w1.get()));
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w3.get()));
}

}  // namespace ash
