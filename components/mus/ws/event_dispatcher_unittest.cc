// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/event_dispatcher.h"

#include "components/mus/public/cpp/event_matcher.h"
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
      : root_(root),
        focused_window_(nullptr),
        last_target_(nullptr),
        last_accelerator_(0),
        last_in_nonclient_area_(false) {}
  ~TestEventDispatcherDelegate() override {}

  mojom::EventPtr GetAndClearLastDispatchedEvent() {
    return last_dispatched_event_.Pass();
  }

  uint32_t GetAndClearLastAccelerator() {
    uint32_t return_value = last_accelerator_;
    last_accelerator_ = 0;
    return return_value;
  }

  ServerWindow* GetAndClearLastTarget() {
    ServerWindow* result = last_target_;
    last_target_ = nullptr;
    return result;
  }
  ServerWindow* last_target() { return last_target_; }

  bool GetAndClearLastInNonclientArea() {
    const bool result = last_in_nonclient_area_;
    last_in_nonclient_area_ = false;
    return result;
  }

  ServerWindow* GetAndClearLastFocusedWindow() {
    ServerWindow* result = focused_window_;
    focused_window_ = nullptr;
    return result;
  }

 private:
  // EventDispatcherDelegate:
  void OnAccelerator(uint32_t accelerator, mojom::EventPtr event) override {
    EXPECT_EQ(0u, last_accelerator_);
    last_accelerator_ = accelerator;
  }
  void SetFocusedWindowFromEventDispatcher(ServerWindow* window) override {
    focused_window_ = window;
  }
  ServerWindow* GetFocusedWindowForEventDispatcher() override {
    return focused_window_;
  }
  void DispatchInputEventToWindow(ServerWindow* target,
                                  bool in_nonclient_area,
                                  mojom::EventPtr event) override {
    last_target_ = target;
    last_dispatched_event_ = event.Pass();
    last_in_nonclient_area_ = in_nonclient_area;
  }

  ServerWindow* root_;
  ServerWindow* focused_window_;
  ServerWindow* last_target_;
  mojom::EventPtr last_dispatched_event_;
  uint32_t last_accelerator_;
  bool last_in_nonclient_area_;

  DISALLOW_COPY_AND_ASSIGN(TestEventDispatcherDelegate);
};

struct MouseEventTest {
  ui::MouseEvent input_event;
  ServerWindow* expected_target_window;
  gfx::Point expected_root_location;
  gfx::Point expected_location;
};

void RunMouseEventTests(EventDispatcher* dispatcher,
                        TestEventDispatcherDelegate* dispatcher_delegate,
                        MouseEventTest* tests,
                        size_t test_count) {
  for (size_t i = 0; i < test_count; ++i) {
    const MouseEventTest& test = tests[i];
    dispatcher->OnEvent(
        mojom::Event::From(static_cast<const ui::Event&>(test.input_event)));

    ASSERT_EQ(test.expected_target_window, dispatcher_delegate->last_target())
        << "Test " << i << " failed.";

    mojom::EventPtr dispatched_event_mojo =
        dispatcher_delegate->GetAndClearLastDispatchedEvent();
    ASSERT_TRUE(dispatched_event_mojo.get()) << "Test " << i << " failed.";
    scoped_ptr<ui::Event> dispatched_event(
        dispatched_event_mojo.To<scoped_ptr<ui::Event>>());
    ASSERT_TRUE(dispatched_event.get()) << "Test " << i << " failed.";
    ASSERT_TRUE(dispatched_event->IsMouseEvent()) << "Test " << i << " failed.";
    EXPECT_FALSE(dispatcher_delegate->GetAndClearLastInNonclientArea())
        << "Test " << i << " failed.";
    ui::MouseEvent* dispatched_mouse_event =
        static_cast<ui::MouseEvent*>(dispatched_event.get());
    EXPECT_EQ(test.expected_root_location,
              dispatched_mouse_event->root_location())
        << "Test " << i << " failed.";
    EXPECT_EQ(test.expected_location, dispatched_mouse_event->location())
        << "Test " << i << " failed.";
  }
}

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
      ui::ET_MOUSE_PRESSED, gfx::Point(20, 25), gfx::Point(20, 25),
      base::TimeDelta(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  dispatcher.OnEvent(
      mojom::Event::From(static_cast<const ui::Event&>(ui_event)));

  ASSERT_EQ(&child, event_dispatcher_delegate.last_target());

  mojom::EventPtr dispatched_event_mojo =
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

TEST(EventDispatcherTest, EventMatching) {
  TestServerWindowDelegate window_delegate;
  ServerWindow root(&window_delegate, WindowId(1, 2));
  TestEventDispatcherDelegate event_dispatcher_delegate(&root);
  EventDispatcher dispatcher(&event_dispatcher_delegate);
  dispatcher.set_root(&root);

  mojom::EventMatcherPtr matcher = mus::CreateKeyMatcher(
      mus::mojom::KEYBOARD_CODE_W, mus::mojom::EVENT_FLAGS_CONTROL_DOWN);
  uint32_t accelerator_1 = 1;
  dispatcher.AddAccelerator(accelerator_1, matcher.Pass());

  ui::KeyEvent key(ui::ET_KEY_PRESSED, ui::VKEY_W, ui::EF_CONTROL_DOWN);
  dispatcher.OnEvent(mojom::Event::From(key));
  EXPECT_EQ(accelerator_1,
            event_dispatcher_delegate.GetAndClearLastAccelerator());

  key = ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_W, ui::EF_NONE);
  dispatcher.OnEvent(mojom::Event::From(key));
  EXPECT_EQ(0u, event_dispatcher_delegate.GetAndClearLastAccelerator());

  uint32_t accelerator_2 = 2;
  matcher = mus::CreateKeyMatcher(mus::mojom::KEYBOARD_CODE_W,
                                  mus::mojom::EVENT_FLAGS_NONE);
  dispatcher.AddAccelerator(accelerator_2, matcher.Pass());
  dispatcher.OnEvent(mojom::Event::From(key));
  EXPECT_EQ(accelerator_2,
            event_dispatcher_delegate.GetAndClearLastAccelerator());

  dispatcher.RemoveAccelerator(accelerator_2);
  dispatcher.OnEvent(mojom::Event::From(key));
  EXPECT_EQ(0u, event_dispatcher_delegate.GetAndClearLastAccelerator());
}

TEST(EventDispatcherTest, Capture) {
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

  MouseEventTest tests[] = {
      // Send a mouse down event over child.
      {ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(20, 25),
                      gfx::Point(20, 25), base::TimeDelta(),
                      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON),
       &child, gfx::Point(20, 25), gfx::Point(10, 15)},
      // Capture should be activated. Let's send a mouse move outside the bounds
      // of the child.
      {ui::MouseEvent(ui::ET_MOUSE_MOVED, gfx::Point(50, 50),
                      gfx::Point(50, 50), base::TimeDelta(),
                      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON),
       &child, gfx::Point(50, 50), gfx::Point(40, 40)},
      // Release the mouse and verify that the mouse up event goes to the child.
      {ui::MouseEvent(ui::ET_MOUSE_RELEASED, gfx::Point(50, 50),
                      gfx::Point(50, 50), base::TimeDelta(),
                      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON),
       &child, gfx::Point(50, 50), gfx::Point(40, 40)},
      // A mouse move at (50, 50) should now go to the root window.
      {ui::MouseEvent(ui::ET_MOUSE_MOVED, gfx::Point(50, 50),
                      gfx::Point(50, 50), base::TimeDelta(),
                      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON),
       &root, gfx::Point(50, 50), gfx::Point(50, 50)},

  };
  RunMouseEventTests(&dispatcher, &event_dispatcher_delegate, tests,
                     arraysize(tests));
}

TEST(EventDispatcherTest, CaptureMultipleMouseButtons) {
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

  MouseEventTest tests[] = {
      // Send a mouse down event over child with a left mouse button
      {ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(20, 25),
                      gfx::Point(20, 25), base::TimeDelta(),
                      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON),
       &child, gfx::Point(20, 25), gfx::Point(10, 15)},
      // Capture should be activated. Let's send a mouse move outside the bounds
      // of the child and press the right mouse button too.
      {ui::MouseEvent(ui::ET_MOUSE_MOVED, gfx::Point(50, 50),
                      gfx::Point(50, 50), base::TimeDelta(),
                      ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON, 0),
       &child, gfx::Point(50, 50), gfx::Point(40, 40)},
      // Release the left mouse button and verify that the mouse up event goes
      // to the child.
      {ui::MouseEvent(ui::ET_MOUSE_RELEASED, gfx::Point(50, 50),
                      gfx::Point(50, 50), base::TimeDelta(),
                      ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON,
                      ui::EF_RIGHT_MOUSE_BUTTON),
       &child, gfx::Point(50, 50), gfx::Point(40, 40)},
      // A mouse move at (50, 50) should still go to the child.
      {ui::MouseEvent(ui::ET_MOUSE_MOVED, gfx::Point(50, 50),
                      gfx::Point(50, 50), base::TimeDelta(),
                      ui::EF_LEFT_MOUSE_BUTTON, 0),
       &child, gfx::Point(50, 50), gfx::Point(40, 40)},

  };
  RunMouseEventTests(&dispatcher, &event_dispatcher_delegate, tests,
                     arraysize(tests));
}

TEST(EventDispatcherTest, ClientAreaGoesToOwner) {
  TestServerWindowDelegate window_delegate;
  ServerWindow root(&window_delegate, WindowId(1, 2));
  window_delegate.set_root_window(&root);
  root.SetVisible(true);

  ServerWindow child(&window_delegate, WindowId(1, 3));
  root.Add(&child);
  child.SetVisible(true);

  root.SetBounds(gfx::Rect(0, 0, 100, 100));
  child.SetBounds(gfx::Rect(10, 10, 20, 20));

  child.SetClientArea(gfx::Insets(5, 5, 5, 5));

  TestEventDispatcherDelegate event_dispatcher_delegate(&root);
  EventDispatcher dispatcher(&event_dispatcher_delegate);
  dispatcher.set_root(&root);

  // Start move loop by sending mouse event over non-client area.
  const ui::MouseEvent press_event(
      ui::ET_MOUSE_PRESSED, gfx::Point(12, 12), gfx::Point(12, 12),
      base::TimeDelta(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  dispatcher.OnEvent(
      mojom::Event::From(static_cast<const ui::Event&>(press_event)));

  // Events should target child and be in the non-client area.
  ASSERT_EQ(&child, event_dispatcher_delegate.last_target());
  EXPECT_TRUE(event_dispatcher_delegate.GetAndClearLastInNonclientArea());

  // Move the mouse 5,6 pixels and target is the same.
  const ui::MouseEvent move_event(ui::ET_MOUSE_MOVED, gfx::Point(17, 18),
                                  gfx::Point(17, 18), base::TimeDelta(),
                                  ui::EF_LEFT_MOUSE_BUTTON, 0);
  dispatcher.OnEvent(
      mojom::Event::From(static_cast<const ui::Event&>(move_event)));

  // Still same target.
  ASSERT_EQ(&child, event_dispatcher_delegate.last_target());
  EXPECT_TRUE(event_dispatcher_delegate.GetAndClearLastInNonclientArea());

  // Release the mouse.
  const ui::MouseEvent release_event(
      ui::ET_MOUSE_RELEASED, gfx::Point(17, 18), gfx::Point(17, 18),
      base::TimeDelta(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  dispatcher.OnEvent(
      mojom::Event::From(static_cast<const ui::Event&>(release_event)));

  // The event should not have been dispatched to the delegate.
  ASSERT_EQ(&child, event_dispatcher_delegate.last_target());
  EXPECT_TRUE(event_dispatcher_delegate.GetAndClearLastInNonclientArea());

  // Press in the client area and verify target/client area.
  const ui::MouseEvent press_event2(
      ui::ET_MOUSE_PRESSED, gfx::Point(21, 22), gfx::Point(21, 22),
      base::TimeDelta(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  dispatcher.OnEvent(
      mojom::Event::From(static_cast<const ui::Event&>(press_event2)));
  ASSERT_EQ(&child, event_dispatcher_delegate.last_target());
  EXPECT_FALSE(event_dispatcher_delegate.GetAndClearLastInNonclientArea());
}

TEST(EventDispatcherTest, DontFocusOnSecondDown) {
  TestServerWindowDelegate window_delegate;
  ServerWindow root(&window_delegate, WindowId(1, 2));
  window_delegate.set_root_window(&root);
  root.SetVisible(true);

  ServerWindow child1(&window_delegate, WindowId(1, 3));
  root.Add(&child1);
  child1.SetVisible(true);

  ServerWindow child2(&window_delegate, WindowId(1, 4));
  root.Add(&child2);
  child2.SetVisible(true);

  root.SetBounds(gfx::Rect(0, 0, 100, 100));
  child1.SetBounds(gfx::Rect(10, 10, 20, 20));
  child2.SetBounds(gfx::Rect(50, 51, 11, 12));

  TestEventDispatcherDelegate event_dispatcher_delegate(&root);
  EventDispatcher dispatcher(&event_dispatcher_delegate);
  dispatcher.set_root(&root);

  // Press on child1. First press event should change focus.
  const ui::MouseEvent press_event(
      ui::ET_MOUSE_PRESSED, gfx::Point(12, 12), gfx::Point(12, 12),
      base::TimeDelta(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  dispatcher.OnEvent(
      mojom::Event::From(static_cast<const ui::Event&>(press_event)));
  EXPECT_EQ(&child1, event_dispatcher_delegate.last_target());
  EXPECT_EQ(&child1, event_dispatcher_delegate.GetAndClearLastFocusedWindow());

  // Press (with a different pointer id) on child2. Event should go to child2,
  // but focus should not change.
  const ui::TouchEvent touch_event(ui::ET_TOUCH_PRESSED, gfx::Point(53, 54), 2,
                                   base::TimeDelta());
  dispatcher.OnEvent(
      mojom::Event::From(static_cast<const ui::Event&>(touch_event)));
  EXPECT_EQ(&child2, event_dispatcher_delegate.last_target());
  EXPECT_EQ(nullptr, event_dispatcher_delegate.GetAndClearLastFocusedWindow());
}

TEST(EventDispatcherTest, TwoPointersActive) {
  TestServerWindowDelegate window_delegate;
  ServerWindow root(&window_delegate, WindowId(1, 2));
  window_delegate.set_root_window(&root);
  root.SetVisible(true);

  ServerWindow child1(&window_delegate, WindowId(1, 3));
  root.Add(&child1);
  child1.SetVisible(true);

  ServerWindow child2(&window_delegate, WindowId(1, 4));
  root.Add(&child2);
  child2.SetVisible(true);

  root.SetBounds(gfx::Rect(0, 0, 100, 100));
  child1.SetBounds(gfx::Rect(10, 10, 20, 20));
  child2.SetBounds(gfx::Rect(50, 51, 11, 12));

  TestEventDispatcherDelegate event_dispatcher_delegate(&root);
  EventDispatcher dispatcher(&event_dispatcher_delegate);
  dispatcher.set_root(&root);

  // Press on child1.
  const ui::TouchEvent touch_event1(ui::ET_TOUCH_PRESSED, gfx::Point(12, 13), 1,
                                    base::TimeDelta());
  dispatcher.OnEvent(
      mojom::Event::From(static_cast<const ui::Event&>(touch_event1)));
  EXPECT_EQ(&child1, event_dispatcher_delegate.GetAndClearLastTarget());

  // Drag over child2, child1 should get the drag.
  const ui::TouchEvent drag_event1(ui::ET_TOUCH_MOVED, gfx::Point(53, 54), 1,
                                   base::TimeDelta());
  dispatcher.OnEvent(
      mojom::Event::From(static_cast<const ui::Event&>(drag_event1)));
  EXPECT_EQ(&child1, event_dispatcher_delegate.GetAndClearLastTarget());

  // Press on child2 with a different touch id.
  const ui::TouchEvent touch_event2(ui::ET_TOUCH_PRESSED, gfx::Point(54, 55), 2,
                                    base::TimeDelta());
  dispatcher.OnEvent(
      mojom::Event::From(static_cast<const ui::Event&>(touch_event2)));
  EXPECT_EQ(&child2, event_dispatcher_delegate.GetAndClearLastTarget());

  // Drag over child1 with id 2, child2 should continue to get the drag.
  const ui::TouchEvent drag_event2(ui::ET_TOUCH_MOVED, gfx::Point(13, 14), 2,
                                   base::TimeDelta());
  dispatcher.OnEvent(
      mojom::Event::From(static_cast<const ui::Event&>(drag_event2)));
  EXPECT_EQ(&child2, event_dispatcher_delegate.GetAndClearLastTarget());

  // Drag again with id 1, child1 should continue to get it.
  dispatcher.OnEvent(
      mojom::Event::From(static_cast<const ui::Event&>(drag_event1)));
  EXPECT_EQ(&child1, event_dispatcher_delegate.GetAndClearLastTarget());

  // Release touch id 1, and click on 2. 2 should get it.
  const ui::TouchEvent touch_release(ui::ET_TOUCH_RELEASED, gfx::Point(54, 55),
                                     1, base::TimeDelta());
  dispatcher.OnEvent(
      mojom::Event::From(static_cast<const ui::Event&>(touch_release)));
  EXPECT_EQ(&child1, event_dispatcher_delegate.GetAndClearLastTarget());
  const ui::TouchEvent touch_event3(ui::ET_TOUCH_PRESSED, gfx::Point(54, 55), 2,
                                    base::TimeDelta());
  dispatcher.OnEvent(
      mojom::Event::From(static_cast<const ui::Event&>(touch_event3)));
  EXPECT_EQ(&child2, event_dispatcher_delegate.GetAndClearLastTarget());
}

TEST(EventDispatcherTest, DestroyWindowWhileGettingEvents) {
  TestServerWindowDelegate window_delegate;
  ServerWindow root(&window_delegate, WindowId(1, 2));
  window_delegate.set_root_window(&root);
  root.SetVisible(true);

  scoped_ptr<ServerWindow> child(
      new ServerWindow(&window_delegate, WindowId(1, 3)));
  root.Add(child.get());
  child->SetVisible(true);

  root.SetBounds(gfx::Rect(0, 0, 100, 100));
  child->SetBounds(gfx::Rect(10, 10, 20, 20));

  TestEventDispatcherDelegate event_dispatcher_delegate(&root);
  EventDispatcher dispatcher(&event_dispatcher_delegate);
  dispatcher.set_root(&root);

  // Press on child.
  const ui::TouchEvent touch_event1(ui::ET_TOUCH_PRESSED, gfx::Point(12, 13), 1,
                                    base::TimeDelta());
  dispatcher.OnEvent(
      mojom::Event::From(static_cast<const ui::Event&>(touch_event1)));
  EXPECT_EQ(child.get(), event_dispatcher_delegate.GetAndClearLastTarget());

  event_dispatcher_delegate.GetAndClearLastDispatchedEvent();

  // Delete child, and continue the drag. Event should not be dispatched.
  child.reset();

  const ui::TouchEvent drag_event1(ui::ET_TOUCH_MOVED, gfx::Point(53, 54), 1,
                                   base::TimeDelta());
  dispatcher.OnEvent(
      mojom::Event::From(static_cast<const ui::Event&>(drag_event1)));
  EXPECT_EQ(nullptr, event_dispatcher_delegate.GetAndClearLastTarget());
  EXPECT_EQ(nullptr,
            event_dispatcher_delegate.GetAndClearLastDispatchedEvent().get());
}

}  // namespace ws
}  // namespace mus
