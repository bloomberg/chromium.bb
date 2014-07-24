// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/magnifier_key_scroller.h"

#include "ash/magnifier/magnification_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_util.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_window_delegate.h"

namespace ash {
namespace {

class KeyEventDelegate : public aura::test::TestWindowDelegate {
 public:
  KeyEventDelegate() {}
  virtual ~KeyEventDelegate() {}

  // ui::EventHandler overrides:
  virtual void OnKeyEvent(ui::KeyEvent* event) OVERRIDE {
    key_event.reset(new ui::KeyEvent(
        event->type(), event->key_code(), event->flags(), false));
  }

  const ui::KeyEvent* event() const { return key_event.get(); }
  void reset() { key_event.reset(); }

 private:
  scoped_ptr<ui::KeyEvent> key_event;

  DISALLOW_COPY_AND_ASSIGN(KeyEventDelegate);
};

}  // namespace

typedef ash::test::AshTestBase MagnifierKeyScrollerTest;

TEST_F(MagnifierKeyScrollerTest, Basic) {
  KeyEventDelegate delegate;
  scoped_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      &delegate,
      0,
      gfx::Rect(10, 10, 100, 100)));
  wm::ActivateWindow(window.get());

  MagnifierKeyScroller::ScopedEnablerForTest scoped;
  Shell* shell = Shell::GetInstance();
  MagnificationController* controller = shell->magnification_controller();
  controller->SetEnabled(true);

  EXPECT_EQ("200,150", controller->GetWindowPosition().ToString());
  aura::test::EventGenerator& generator = GetEventGenerator();

  // Click and Release generates the press event upon release.
  generator.PressKey(ui::VKEY_DOWN, ui::EF_SHIFT_DOWN);
  EXPECT_EQ("200,150", controller->GetWindowPosition().ToString());
  EXPECT_FALSE(delegate.event());

  generator.ReleaseKey(ui::VKEY_DOWN, 0);
  EXPECT_EQ("200,150", controller->GetWindowPosition().ToString());
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(delegate.event());
  EXPECT_EQ(ui::ET_KEY_PRESSED, delegate.event()->type());
  delegate.reset();

  // Click and hold scrolls the magnifier screen.
  generator.PressKey(ui::VKEY_DOWN, ui::EF_SHIFT_DOWN);
  EXPECT_EQ("200,150", controller->GetWindowPosition().ToString());
  EXPECT_FALSE(delegate.event());

  generator.PressKey(ui::VKEY_DOWN, ui::EF_SHIFT_DOWN);
  EXPECT_EQ("200,300", controller->GetWindowPosition().ToString());
  EXPECT_FALSE(delegate.event());

  generator.ReleaseKey(ui::VKEY_DOWN, 0);
  EXPECT_EQ("200,300", controller->GetWindowPosition().ToString());
  EXPECT_FALSE(delegate.event());

  // Events are passed normally when the magnifier is off.
  controller->SetEnabled(false);

  generator.PressKey(ui::VKEY_DOWN, ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(delegate.event());
  EXPECT_EQ(ui::ET_KEY_PRESSED, delegate.event()->type());
  delegate.reset();

  generator.ReleaseKey(ui::VKEY_DOWN, 0);
  EXPECT_TRUE(delegate.event());
  EXPECT_EQ(ui::ET_KEY_RELEASED, delegate.event()->type());
  delegate.reset();

  generator.PressKey(ui::VKEY_DOWN, ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(delegate.event());
  EXPECT_EQ(ui::ET_KEY_PRESSED, delegate.event()->type());
  delegate.reset();

  generator.PressKey(ui::VKEY_DOWN, ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(delegate.event());
  EXPECT_EQ(ui::ET_KEY_PRESSED, delegate.event()->type());
  delegate.reset();

  generator.ReleaseKey(ui::VKEY_DOWN, 0);
  EXPECT_TRUE(delegate.event());
  EXPECT_EQ(ui::ET_KEY_RELEASED, delegate.event()->type());
  delegate.reset();
}

}  // namespace ash
