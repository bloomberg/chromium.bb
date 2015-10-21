// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/event_dispatcher.h"

#include "components/mus/ws/event_dispatcher_delegate.h"
#include "components/mus/ws/server_window.h"
#include "components/mus/ws/test_server_window_delegate.h"
#include "mojo/converters/input_events/input_events_type_converters.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"

namespace mus {
namespace ws {
namespace {

class TestEventDispatcherDelegate : public EventDispatcherDelegate {
 public:
  explicit TestEventDispatcherDelegate(ServerWindow* root)
      : root_(root), focused_window_(nullptr), last_target_(nullptr) {}
  ~TestEventDispatcherDelegate() override {}

  mojo::EventPtr GetAndClearLastDispatchedEvent() {
    return last_dispatched_event_.Pass();
  }

  ServerWindow* last_target() { return last_target_; }

 private:
  // EventDispatcherDelegate:
  void OnAccelerator(uint32_t accelerator, mojo::EventPtr event) override {}
  void SetFocusedWindowFromEventDispatcher(ServerWindow* window) override {
    focused_window_ = window;
  }
  ServerWindow* GetFocusedWindowForEventDispatcher() override {
    return focused_window_;
  }
  void DispatchInputEventToWindow(ServerWindow* target,
                                  mojo::EventPtr event) override {
    last_target_ = target;
    last_dispatched_event_ = event.Pass();
  }

  ServerWindow* root_;
  ServerWindow* focused_window_;
  ServerWindow* last_target_;
  mojo::EventPtr last_dispatched_event_;

  DISALLOW_COPY_AND_ASSIGN(TestEventDispatcherDelegate);
};

}  // namespace

TEST(EventDispatcherTest, OnEvent) {
  TestServerWindowDelegate window_delegate;
  ServerWindow root(&window_delegate, WindowId(1, 2));
  window_delegate.set_root_window(&root);
  root.SetVisible(true);

  ServerWindow child(&window_delegate, WindowId(1, 3));
  root.Add(&child);
  child.SetVisible(true);

  root.SetBounds(gfx::Rect(0, 0, 100, 100));
  child.SetBounds(gfx::Rect(10, 10, 20, 20));

  TestEventDispatcherDelegate event_dispatcher_delegate(&root);
  EventDispatcher dispatcher(&event_dispatcher_delegate);
  dispatcher.set_root(&root);

  // Send event that is over child.
  const ui::MouseEvent ui_event(
      ui::ET_MOUSE_PRESSED, gfx::PointF(20.f, 25.f), gfx::PointF(20.f, 25.f),
      base::TimeDelta(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  dispatcher.OnEvent(
      mojo::Event::From(static_cast<const ui::Event&>(ui_event)));

  ASSERT_EQ(&child, event_dispatcher_delegate.last_target());

  mojo::EventPtr dispatched_event_mojo =
      event_dispatcher_delegate.GetAndClearLastDispatchedEvent();
  ASSERT_TRUE(dispatched_event_mojo.get());
  scoped_ptr<ui::Event> dispatched_event(
      dispatched_event_mojo.To<scoped_ptr<ui::Event>>());
  ASSERT_TRUE(dispatched_event.get());
  ASSERT_TRUE(dispatched_event->IsMouseEvent());
  ui::MouseEvent* dispatched_mouse_event =
      static_cast<ui::MouseEvent*>(dispatched_event.get());
  EXPECT_EQ(gfx::Point(20, 25), dispatched_mouse_event->root_location());
  EXPECT_EQ(gfx::Point(10, 15), dispatched_mouse_event->location());
}

}  // namespace ws
}  // namespace mus
