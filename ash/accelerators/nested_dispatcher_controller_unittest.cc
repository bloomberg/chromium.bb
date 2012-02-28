// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "base/bind.h"
#include "base/event_types.h"
#include "base/message_loop.h"
#include "chrome/test/base/ui_test_utils.h"
#include "ui/aura/client/dispatcher_client.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"

#if defined(USE_X11)
#include <X11/Xlib.h>
#include "ui/base/x/x11_util.h"
#endif  // USE_X11

namespace ash {
namespace test {

namespace {

class MockDispatcher : public MessageLoop::Dispatcher {
 public:
  MockDispatcher() : num_key_events_dispatched_(0) {
  }

  int num_key_events_dispatched() { return num_key_events_dispatched_; }

#if defined(OS_WIN)
  virtual bool Dispatch(const MSG& msg) OVERRIDE {
    if (msg.message == WM_KEYUP)
      num_key_events_dispatched_++;
    return !ui::IsNoopEvent(msg);
  }
#elif defined(USE_X11)
  virtual base::MessagePumpDispatcher::DispatchStatus Dispatch(
      XEvent* xev) OVERRIDE {
    if (xev->type == KeyRelease)
      num_key_events_dispatched_++;
    return ui::IsNoopEvent(xev) ? MessagePumpDispatcher::EVENT_QUIT :
        MessagePumpDispatcher::EVENT_IGNORED;
  }
#endif

 private:
  int num_key_events_dispatched_;
};

void DispatchKeyEvent() {
#if defined(OS_WIN)
  MSG native_event = { NULL, WM_KEYUP, ui::VKEY_A, 0 };
  ash::Shell::GetRootWindow()->PostNativeEvent(native_event);
#elif defined(USE_X11)
  XEvent native_event;
  ui::InitXKeyEventForTesting(ui::ET_KEY_RELEASED,
                              ui::VKEY_A,
                              0,
                              &native_event);
  ash::Shell::GetRootWindow()->PostNativeEvent(&native_event);
#endif

  // Send noop event to signal dispatcher to exit.
  ash::Shell::GetRootWindow()->PostNativeEvent(ui::CreateNoopEvent());
}

}  // namespace

typedef AshTestBase NestedDispatcherTest;

// Aura window below lock screen in z order.
TEST_F(NestedDispatcherTest, AssociatedWindowBelowLockScreen) {
  MockDispatcher inner_dispatcher;
  aura::Window* default_container = Shell::GetInstance()->GetContainer(
      ash::internal::kShellWindowId_DefaultContainer);
  scoped_ptr<aura::Window>associated_window(aura::test::CreateTestWindowWithId(
      0, default_container));
  scoped_ptr<aura::Window>mock_lock_container(
      aura::test::CreateTestWindowWithId(0, default_container));
  mock_lock_container->set_stops_event_propagation(true);
  aura::test::CreateTestWindowWithId(0, mock_lock_container.get());
  EXPECT_TRUE(aura::test::WindowIsAbove(mock_lock_container.get(),
      associated_window.get()));
  MessageLoop::current()->PostDelayedTask(
     FROM_HERE,
     base::Bind(&DispatchKeyEvent),
     base::TimeDelta::FromMilliseconds(100));
  aura::RootWindow* root_window = ash::Shell::GetInstance()->GetRootWindow();
  aura::client::GetDispatcherClient(root_window)->RunWithDispatcher(
      &inner_dispatcher,
      associated_window.get(),
      true /* nestable_tasks_allowed */);
  EXPECT_EQ(0, inner_dispatcher.num_key_events_dispatched());
}

// Aura window above lock screen in z order.
TEST_F(NestedDispatcherTest, AssociatedWindowAboveLockScreen) {
  MockDispatcher inner_dispatcher;

  aura::Window* default_container = Shell::GetInstance()->GetContainer(
      ash::internal::kShellWindowId_DefaultContainer);
  scoped_ptr<aura::Window>mock_lock_container(
      aura::test::CreateTestWindowWithId(0, default_container));
  mock_lock_container->set_stops_event_propagation(true);
  aura::test::CreateTestWindowWithId(0, mock_lock_container.get());
  scoped_ptr<aura::Window>associated_window(aura::test::CreateTestWindowWithId(
      0, default_container));
  EXPECT_TRUE(aura::test::WindowIsAbove(associated_window.get(),
      mock_lock_container.get()));

  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&DispatchKeyEvent),
      base::TimeDelta::FromMilliseconds(100));
  aura::RootWindow* root_window = ash::Shell::GetInstance()->GetRootWindow();
  aura::client::GetDispatcherClient(root_window)->RunWithDispatcher(
      &inner_dispatcher,
      associated_window.get(),
      true /* nestable_tasks_allowed */);
  EXPECT_EQ(1, inner_dispatcher.num_key_events_dispatched());
}

} //  namespace test
} //  namespace ash
