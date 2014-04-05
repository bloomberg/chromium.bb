// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_controller.h"
#include "ash/session_state_delegate.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "base/bind.h"
#include "base/event_types.h"
#include "base/message_loop/message_loop.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/platform/scoped_event_dispatcher.h"
#include "ui/wm/public/dispatcher_client.h"

#if defined(USE_X11)
#include <X11/Xlib.h>
#include "ui/events/test/events_test_utils_x11.h"
#endif  // USE_X11

namespace ash {
namespace test {

namespace {

class MockDispatcher : public ui::PlatformEventDispatcher {
 public:
  MockDispatcher() : num_key_events_dispatched_(0) {
  }

  int num_key_events_dispatched() { return num_key_events_dispatched_; }

 private:
  // ui::PlatformEventDispatcher:
  virtual bool CanDispatchEvent(const ui::PlatformEvent& event) OVERRIDE {
    return true;
  }
  virtual uint32_t DispatchEvent(const ui::PlatformEvent& event) OVERRIDE {
    if (ui::EventTypeFromNative(event) == ui::ET_KEY_RELEASED)
      num_key_events_dispatched_++;
    return ui::POST_DISPATCH_NONE;
  }

  int num_key_events_dispatched_;

  DISALLOW_COPY_AND_ASSIGN(MockDispatcher);
};

class TestTarget : public ui::AcceleratorTarget {
 public:
  TestTarget() : accelerator_pressed_count_(0) {
  }
  virtual ~TestTarget() {
  }

  int accelerator_pressed_count() const {
    return accelerator_pressed_count_;
  }

  // Overridden from ui::AcceleratorTarget:
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE {
    accelerator_pressed_count_++;
    return true;
  }
  virtual bool CanHandleAccelerators() const OVERRIDE {
    return true;
  }

 private:
  int accelerator_pressed_count_;

  DISALLOW_COPY_AND_ASSIGN(TestTarget);
};

void DispatchKeyReleaseA() {
  // Sending both keydown and keyup is necessary here because the accelerator
  // manager only checks a keyup event following a keydown event. See
  // ShouldHandle() in ui/base/accelerators/accelerator_manager.cc for details.
#if defined(OS_WIN)
  MSG native_event_down = { NULL, WM_KEYDOWN, ui::VKEY_A, 0 };
  ash::Shell::GetPrimaryRootWindow()->host()->PostNativeEvent(
      native_event_down);
  MSG native_event_up = { NULL, WM_KEYUP, ui::VKEY_A, 0 };
  ash::Shell::GetPrimaryRootWindow()->host()->PostNativeEvent(native_event_up);
#elif defined(USE_X11)
  ui::ScopedXI2Event native_event;
  native_event.InitKeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_A, 0);
  aura::WindowTreeHost* host = ash::Shell::GetPrimaryRootWindow()->GetHost();
  host->PostNativeEvent(native_event);
  native_event.InitKeyEvent(ui::ET_KEY_RELEASED, ui::VKEY_A, 0);
  host->PostNativeEvent(native_event);
#endif
  // Make sure the inner message-loop terminates after dispatching the events.
  base::MessageLoop::current()->PostTask(FROM_HERE,
      base::MessageLoop::current()->QuitClosure());
}

}  // namespace

typedef AshTestBase NestedDispatcherTest;

// Aura window above lock screen in z order.
TEST_F(NestedDispatcherTest, AssociatedWindowAboveLockScreen) {
  MockDispatcher inner_dispatcher;

  scoped_ptr<aura::Window> mock_lock_container(
      CreateTestWindowInShellWithId(0));
  aura::test::CreateTestWindowWithId(1, mock_lock_container.get());
  scoped_ptr<aura::Window> associated_window(CreateTestWindowInShellWithId(2));
  EXPECT_TRUE(aura::test::WindowIsAbove(associated_window.get(),
      mock_lock_container.get()));

  DispatchKeyReleaseA();
  aura::Window* root_window = ash::Shell::GetPrimaryRootWindow();
  scoped_ptr<ui::ScopedEventDispatcher> override_dispatcher =
      ui::PlatformEventSource::GetInstance()->OverrideDispatcher(
          &inner_dispatcher);
  aura::client::GetDispatcherClient(root_window)->RunWithDispatcher(NULL);
  EXPECT_EQ(1, inner_dispatcher.num_key_events_dispatched());
}

// Test that the nested dispatcher handles accelerators.
TEST_F(NestedDispatcherTest, AcceleratorsHandled) {
  MockDispatcher inner_dispatcher;
  aura::Window* root_window = ash::Shell::GetPrimaryRootWindow();

  ui::Accelerator accelerator(ui::VKEY_A, ui::EF_NONE);
  accelerator.set_type(ui::ET_KEY_RELEASED);
  TestTarget target;
  Shell::GetInstance()->accelerator_controller()->Register(accelerator,
                                                           &target);

  DispatchKeyReleaseA();
  scoped_ptr<ui::ScopedEventDispatcher> override_dispatcher =
      ui::PlatformEventSource::GetInstance()->OverrideDispatcher(
          &inner_dispatcher);
  aura::client::GetDispatcherClient(root_window)->RunWithDispatcher(NULL);
  EXPECT_EQ(0, inner_dispatcher.num_key_events_dispatched());
  EXPECT_EQ(1, target.accelerator_pressed_count());
}

} //  namespace test
} //  namespace ash
