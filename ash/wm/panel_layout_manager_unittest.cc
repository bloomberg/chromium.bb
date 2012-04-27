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
#include "ash/wm/window_util.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "ui/aura/window.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/test/test_window_delegate.h"

namespace ash {

namespace {

aura::Window* GetPanelContainer() {
  return Shell::GetInstance()->GetContainer(
      ash::internal::kShellWindowId_PanelContainer);
}

class PanelLayoutManagerTest : public ash::test::AshTestBase {
 public:
  PanelLayoutManagerTest() {}
  virtual ~PanelLayoutManagerTest() {}

  virtual void SetUp() OVERRIDE {
    CommandLine::ForCurrentProcess()->AppendSwitch(switches::kAuraPanelManager);
    ash::test::AshTestBase::SetUp();
    ASSERT_TRUE(ash::test::TestLauncherDelegate::instance());
  }

  aura::Window* CreatePanelWindow(const gfx::Rect& bounds) {
    aura::Window* window = CreateTestWindowWithDelegateAndType(
        &window_delegate_,
        aura::client::WINDOW_TYPE_PANEL,
        0,
        bounds,
        NULL /* parent should automatically become GetPanelContainer */);
    ash::test::TestLauncherDelegate* launcher_delegate =
        ash::test::TestLauncherDelegate::instance();
    launcher_delegate->AddLauncherItem(window);
    return window;
  }

 private:
  aura::test::TestWindowDelegate window_delegate_;

  DISALLOW_COPY_AND_ASSIGN(PanelLayoutManagerTest);
};

// TODO(dcheng): This should be const, but GetScreenBoundsOfItemIconForWindow
// takes a non-const Window. We can probably fix that.
void IsPanelAboveLauncherIcon(aura::Window* panel) {
  Launcher* launcher = Shell::GetInstance()->launcher();
  gfx::Rect icon_bounds = launcher->GetScreenBoundsOfItemIconForWindow(panel);
  ASSERT_FALSE(icon_bounds.IsEmpty());

  gfx::Rect window_bounds = panel->GetBoundsInRootWindow();

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
  gfx::Rect bounds(0, 0, 201, 201);
  scoped_ptr<aura::Window> window(CreatePanelWindow(bounds));
  EXPECT_EQ(GetPanelContainer(), window->parent());
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(window.get()));
}

// Tests interactions between multiple panels
TEST_F(PanelLayoutManagerTest, MultiplePanelsAreAboveIcons) {
  gfx::Rect odd_bounds(0, 0, 201, 201);
  gfx::Rect even_bounds(0, 0, 200, 200);

  scoped_ptr<aura::Window> w1(CreatePanelWindow(odd_bounds));
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w1.get()));

  scoped_ptr<aura::Window> w2(CreatePanelWindow(even_bounds));
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w1.get()));
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w2.get()));

  scoped_ptr<aura::Window> w3(CreatePanelWindow(odd_bounds));
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w1.get()));
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w2.get()));
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w3.get()));
}

TEST_F(PanelLayoutManagerTest, MultiplePanelStacking) {
  gfx::Rect bounds(0, 0, 201, 201);
  scoped_ptr<aura::Window> w1(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> w2(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> w3(CreatePanelWindow(bounds));

  // Default stacking order.
  ASSERT_EQ(3u, GetPanelContainer()->children().size());
  EXPECT_EQ(w1.get(), GetPanelContainer()->children()[0]);
  EXPECT_EQ(w2.get(), GetPanelContainer()->children()[1]);
  EXPECT_EQ(w3.get(), GetPanelContainer()->children()[2]);

  // Changing the active window should update the stacking order.
  wm::ActivateWindow(w1.get());
  ASSERT_EQ(3u, GetPanelContainer()->children().size());
  EXPECT_EQ(w3.get(), GetPanelContainer()->children()[0]);
  EXPECT_EQ(w2.get(), GetPanelContainer()->children()[1]);
  EXPECT_EQ(w1.get(), GetPanelContainer()->children()[2]);

  wm::ActivateWindow(w2.get());
  ASSERT_EQ(3u, GetPanelContainer()->children().size());
  EXPECT_EQ(w3.get(), GetPanelContainer()->children()[0]);
  EXPECT_EQ(w1.get(), GetPanelContainer()->children()[1]);
  EXPECT_EQ(w2.get(), GetPanelContainer()->children()[2]);

  wm::ActivateWindow(w3.get());
  ASSERT_EQ(3u, GetPanelContainer()->children().size());
  EXPECT_EQ(w1.get(), GetPanelContainer()->children()[0]);
  EXPECT_EQ(w2.get(), GetPanelContainer()->children()[1]);
  EXPECT_EQ(w3.get(), GetPanelContainer()->children()[2]);
}

// Tests removing panels.
TEST_F(PanelLayoutManagerTest, RemoveLeftPanel) {
  gfx::Rect bounds(0, 0, 201, 201);
  scoped_ptr<aura::Window> w1(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> w2(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> w3(CreatePanelWindow(bounds));

  wm::ActivateWindow(w1.get());
  w1.reset();
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w2.get()));
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w3.get()));
  ASSERT_EQ(2u, GetPanelContainer()->children().size());
  EXPECT_EQ(w3.get(), GetPanelContainer()->children()[0]);
  EXPECT_EQ(w2.get(), GetPanelContainer()->children()[1]);
}

TEST_F(PanelLayoutManagerTest, RemoveMiddlePanel) {
  gfx::Rect bounds(0, 0, 201, 201);
  scoped_ptr<aura::Window> w1(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> w2(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> w3(CreatePanelWindow(bounds));

  wm::ActivateWindow(w2.get());
  w2.reset();
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w1.get()));
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w3.get()));
  ASSERT_EQ(2u, GetPanelContainer()->children().size());
  EXPECT_EQ(w1.get(), GetPanelContainer()->children()[0]);
  EXPECT_EQ(w3.get(), GetPanelContainer()->children()[1]);
}

TEST_F(PanelLayoutManagerTest, RemoveRightPanel) {
  gfx::Rect bounds(0, 0, 201, 201);
  scoped_ptr<aura::Window> w1(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> w2(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> w3(CreatePanelWindow(bounds));

  wm::ActivateWindow(w3.get());
  w3.reset();
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w1.get()));
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w2.get()));
  ASSERT_EQ(2u, GetPanelContainer()->children().size());
  EXPECT_EQ(w1.get(), GetPanelContainer()->children()[0]);
  EXPECT_EQ(w2.get(), GetPanelContainer()->children()[1]);
}

TEST_F(PanelLayoutManagerTest, RemoveNonActivePanel) {
  gfx::Rect bounds(0, 0, 201, 201);
  scoped_ptr<aura::Window> w1(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> w2(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> w3(CreatePanelWindow(bounds));

  wm::ActivateWindow(w2.get());
  w1.reset();
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w2.get()));
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w3.get()));
  ASSERT_EQ(2u, GetPanelContainer()->children().size());
  EXPECT_EQ(w3.get(), GetPanelContainer()->children()[0]);
  EXPECT_EQ(w2.get(), GetPanelContainer()->children()[1]);
}

}  // namespace ash
