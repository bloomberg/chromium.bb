// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ime/input_method_event_filter.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/aura_shell_test_base.h"
#include "ash/wm/root_window_event_filter.h"
#include "ash/wm/window_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_event_filter.h"
#include "ui/aura/test/test_windows.h"

#if !defined(OS_WIN) && !defined(USE_X11)
// On platforms except Windows and X11, aura::test::EventGenerator::PressKey
// generates a key event without native_event(), which is not supported by
// ui::MockInputMethod.
#define TestInputMethodKeyEventPropagation \
DISABLED_TestInputMethodKeyEventPropagation
#endif

namespace ash {
namespace test {

typedef aura::test::AuraTestBase InputMethodEventFilterTestWithoutShell;
typedef AuraShellTestBase InputMethodEventFilterTest;

// Tests if InputMethodEventFilter adds a window property on its construction.
TEST_F(InputMethodEventFilterTestWithoutShell, TestInputMethodProperty) {
  aura::RootWindow* root_window = aura::RootWindow::GetInstance();
  internal::RootWindowEventFilter* root_filter =
      new internal::RootWindowEventFilter;
  EXPECT_FALSE(root_window->GetProperty(aura::client::kRootWindowInputMethod));
  internal::InputMethodEventFilter ime_filter;
  root_filter->AddFilter(&ime_filter);
  EXPECT_TRUE(root_window->GetProperty(aura::client::kRootWindowInputMethod));
  root_filter->RemoveFilter(&ime_filter);
}

// Tests if InputMethodEventFilter dispatches a ui::ET_TRANSLATED_KEY_* event to
// the root window.
TEST_F(InputMethodEventFilterTest, TestInputMethodKeyEventPropagation) {
  aura::RootWindow* root_window = aura::RootWindow::GetInstance();

  // Add TestEventFilter to the RootWindow.
  aura::test::TestEventFilter test_filter(root_window);
  internal::RootWindowEventFilter* root_filter =
      static_cast<internal::RootWindowEventFilter*>(
          root_window->event_filter());
  ASSERT_TRUE(root_filter);
  root_filter->AddFilter(&test_filter);

  // We need an active window. Otherwise, the root window will not forward a key
  // event to event filters.
  aura::Window* default_container = Shell::GetInstance()->GetContainer(
      internal::kShellWindowId_DefaultContainer);
  aura::Window* window = aura::test::CreateTestWindowWithDelegate(
      new aura::test::TestWindowDelegate,
      -1,
      gfx::Rect(),
      default_container);
  ActivateWindow(window);

  // Send a fake key event to the root window. InputMethodEventFilter, which is
  // automatically set up by AuraShellTestBase, consumes it and sends a new
  // ui::ET_TRANSLATED_KEY_* event to the root window, which will be consumed by
  // the test event filter.
  aura::test::EventGenerator generator_;
  EXPECT_EQ(0, test_filter.key_event_count());
  generator_.PressKey(ui::VKEY_SPACE, 0);
  EXPECT_EQ(1, test_filter.key_event_count());
  generator_.ReleaseKey(ui::VKEY_SPACE, 0);
  EXPECT_EQ(2, test_filter.key_event_count());

  root_filter->RemoveFilter(&test_filter);
}

}  // namespace test
}  // namespace ash
