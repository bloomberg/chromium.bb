// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/event_dispatcher.h"

#include <stddef.h>
#include <stdint.h>

#include <queue>

#include "base/macros.h"
#include "components/mus/public/cpp/event_matcher.h"
#include "components/mus/ws/event_dispatcher_delegate.h"
#include "components/mus/ws/server_window.h"
#include "components/mus/ws/server_window_surface_manager_test_api.h"
#include "components/mus/ws/test_server_window_delegate.h"
#include "mojo/converters/input_events/input_events_type_converters.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"

namespace mus {
namespace ws {
namespace {

// Identifies a generated event.
struct DispatchedEventDetails {
  DispatchedEventDetails() : window(nullptr), in_nonclient_area(false) {}

  ServerWindow* window;
  bool in_nonclient_area;
  mojom::EventPtr event;
};

class TestEventDispatcherDelegate : public EventDispatcherDelegate {
 public:
  explicit TestEventDispatcherDelegate(ServerWindow* root)
      : root_(root), focused_window_(nullptr), last_accelerator_(0) {}
  ~TestEventDispatcherDelegate() override {}

  uint32_t GetAndClearLastAccelerator() {
    uint32_t return_value = last_accelerator_;
    last_accelerator_ = 0;
    return return_value;
  }

  // Returns the last dispatched event, or null if there are no more.
  scoped_ptr<DispatchedEventDetails> GetAndAdvanceDispatchedEventDetails() {
    if (dispatched_event_queue_.empty())
      return nullptr;

    scoped_ptr<DispatchedEventDetails> details =
        std::move(dispatched_event_queue_.front());
    dispatched_event_queue_.pop();
    return details;
  }

  ServerWindow* GetAndClearLastFocusedWindow() {
    ServerWindow* result = focused_window_;
    focused_window_ = nullptr;
    return result;
  }

  bool has_queued_events() const { return !dispatched_event_queue_.empty(); }

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
    scoped_ptr<DispatchedEventDetails> details(new DispatchedEventDetails);
    details->window = target;
    details->in_nonclient_area = in_nonclient_area;
    details->event = std::move(event);
    dispatched_event_queue_.push(std::move(details));
  }

  ServerWindow* root_;
  ServerWindow* focused_window_;
  uint32_t last_accelerator_;
  std::queue<scoped_ptr<DispatchedEventDetails>> dispatched_event_queue_;

  DISALLOW_COPY_AND_ASSIGN(TestEventDispatcherDelegate);
};

// Used by RunMouseEventTests(). Can identify up to two generated events. The
// first ServerWindow and two points identify the first event, the second
// ServerWindow and points identify the second event. If only one event is
// generated set the second window to null.
struct MouseEventTest {
  ui::MouseEvent input_event;
  ServerWindow* expected_target_window1;
  gfx::Point expected_root_location1;
  gfx::Point expected_location1;
  ServerWindow* expected_target_window2;
  gfx::Point expected_root_location2;
  gfx::Point expected_location2;
};

// Verifies |details| matches the supplied ServerWindow and points.
void ExpectDispatchedEventDetailsMatches(const DispatchedEventDetails* details,
                                         ServerWindow* target,
                                         const gfx::Point& root_location,
                                         const gfx::Point& location) {
  if (!target) {
    ASSERT_FALSE(details);
    return;
  }

  ASSERT_EQ(target, details->window);
  scoped_ptr<ui::Event> dispatched_event(
      details->event.To<scoped_ptr<ui::Event>>());
  ASSERT_TRUE(dispatched_event);
  ASSERT_TRUE(dispatched_event->IsMouseEvent());
  ASSERT_FALSE(details->in_nonclient_area);
  ui::MouseEvent* dispatched_mouse_event =
      static_cast<ui::MouseEvent*>(dispatched_event.get());
  ASSERT_EQ(root_location, dispatched_mouse_event->root_location());
  ASSERT_EQ(location, dispatched_mouse_event->location());
}

void RunMouseEventTests(EventDispatcher* dispatcher,
                        TestEventDispatcherDelegate* dispatcher_delegate,
                        MouseEventTest* tests,
                        size_t test_count) {
  for (size_t i = 0; i < test_count; ++i) {
    const MouseEventTest& test = tests[i];
    ASSERT_FALSE(dispatcher_delegate->has_queued_events())
        << " unexpected queued events before running " << i;
    dispatcher->ProcessEvent(
        mojom::Event::From(static_cast<const ui::Event&>(test.input_event)));

    scoped_ptr<DispatchedEventDetails> details =
        dispatcher_delegate->GetAndAdvanceDispatchedEventDetails();
    ASSERT_NO_FATAL_FAILURE(ExpectDispatchedEventDetailsMatches(
        details.get(), test.expected_target_window1,
        test.expected_root_location1, test.expected_location1))
        << " details don't match " << i;
    details = dispatcher_delegate->GetAndAdvanceDispatchedEventDetails();
    ASSERT_NO_FATAL_FAILURE(ExpectDispatchedEventDetailsMatches(
        details.get(), test.expected_target_window2,
        test.expected_root_location2, test.expected_location2))
        << " details2 don't match " << i;
    ASSERT_FALSE(dispatcher_delegate->has_queued_events())
        << " unexpected queued events after running " << i;
  }
}

}  // namespace

TEST(EventDispatcherTest, ProcessEvent) {
  TestServerWindowDelegate window_delegate;
  ServerWindow root(&window_delegate, WindowId(1, 2));
  window_delegate.set_root_window(&root);
  root.SetVisible(true);

  ServerWindow child(&window_delegate, WindowId(1, 3));
  root.Add(&child);
  child.SetVisible(true);
  EnableHitTest(&child);

  root.SetBounds(gfx::Rect(0, 0, 100, 100));
  child.SetBounds(gfx::Rect(10, 10, 20, 20));

  TestEventDispatcherDelegate event_dispatcher_delegate(&root);
  EventDispatcher dispatcher(&event_dispatcher_delegate);
  dispatcher.set_root(&root);

  // Send event that is over child.
  const ui::MouseEvent ui_event(
      ui::ET_MOUSE_PRESSED, gfx::Point(20, 25), gfx::Point(20, 25),
      base::TimeDelta(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  dispatcher.ProcessEvent(
      mojom::Event::From(static_cast<const ui::Event&>(ui_event)));

  scoped_ptr<DispatchedEventDetails> details =
      event_dispatcher_delegate.GetAndAdvanceDispatchedEventDetails();

  ASSERT_TRUE(details);
  ASSERT_EQ(&child, details->window);

  scoped_ptr<ui::Event> dispatched_event(
      details->event.To<scoped_ptr<ui::Event>>());
  ASSERT_TRUE(dispatched_event);
  ASSERT_TRUE(dispatched_event->IsMouseEvent());
  ui::MouseEvent* dispatched_mouse_event =
      static_cast<ui::MouseEvent*>(dispatched_event.get());
  EXPECT_EQ(gfx::Point(20, 25), dispatched_mouse_event->root_location());
  EXPECT_EQ(gfx::Point(10, 15), dispatched_mouse_event->location());
}

TEST(EventDispatcherTest, AcceleratorBasic) {
  TestEventDispatcherDelegate event_dispatcher_delegate(nullptr);
  EventDispatcher dispatcher(&event_dispatcher_delegate);
  uint32_t accelerator_1 = 1;
  mojom::EventMatcherPtr matcher = mus::CreateKeyMatcher(
      mus::mojom::KeyboardCode::W, mus::mojom::kEventFlagControlDown);
  EXPECT_TRUE(dispatcher.AddAccelerator(accelerator_1, std::move(matcher)));

  uint32_t accelerator_2 = 2;
  matcher = mus::CreateKeyMatcher(mus::mojom::KeyboardCode::N,
                                  mus::mojom::kEventFlagNone);
  EXPECT_TRUE(dispatcher.AddAccelerator(accelerator_2, std::move(matcher)));

  // Attempting to add a new accelerator with the same id should fail.
  matcher = mus::CreateKeyMatcher(mus::mojom::KeyboardCode::T,
                                  mus::mojom::kEventFlagNone);
  EXPECT_FALSE(dispatcher.AddAccelerator(accelerator_2, std::move(matcher)));

  // Adding the accelerator with the same id should succeed once the existing
  // accelerator is removed.
  dispatcher.RemoveAccelerator(accelerator_2);
  matcher = mus::CreateKeyMatcher(mus::mojom::KeyboardCode::T,
                                  mus::mojom::kEventFlagNone);
  EXPECT_TRUE(dispatcher.AddAccelerator(accelerator_2, std::move(matcher)));

  // Attempting to add an accelerator with the same matcher should fail.
  uint32_t accelerator_3 = 3;
  matcher = mus::CreateKeyMatcher(mus::mojom::KeyboardCode::T,
                                  mus::mojom::kEventFlagNone);
  EXPECT_FALSE(dispatcher.AddAccelerator(accelerator_3, std::move(matcher)));

  matcher = mus::CreateKeyMatcher(mus::mojom::KeyboardCode::T,
                                  mus::mojom::kEventFlagControlDown);
  EXPECT_TRUE(dispatcher.AddAccelerator(accelerator_3, std::move(matcher)));
}

TEST(EventDispatcherTest, EventMatching) {
  TestServerWindowDelegate window_delegate;
  ServerWindow root(&window_delegate, WindowId(1, 2));
  TestEventDispatcherDelegate event_dispatcher_delegate(&root);
  EventDispatcher dispatcher(&event_dispatcher_delegate);
  dispatcher.set_root(&root);

  mojom::EventMatcherPtr matcher = mus::CreateKeyMatcher(
      mus::mojom::KeyboardCode::W, mus::mojom::kEventFlagControlDown);
  uint32_t accelerator_1 = 1;
  dispatcher.AddAccelerator(accelerator_1, std::move(matcher));

  ui::KeyEvent key(ui::ET_KEY_PRESSED, ui::VKEY_W, ui::EF_CONTROL_DOWN);
  dispatcher.ProcessEvent(mojom::Event::From(key));
  EXPECT_EQ(accelerator_1,
            event_dispatcher_delegate.GetAndClearLastAccelerator());

  // EF_NUM_LOCK_ON should be ignored since CreateKeyMatcher defaults to
  // ignoring.
  key = ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_W,
                     ui::EF_CONTROL_DOWN | ui::EF_NUM_LOCK_ON);
  dispatcher.ProcessEvent(mojom::Event::From(key));
  EXPECT_EQ(accelerator_1,
            event_dispatcher_delegate.GetAndClearLastAccelerator());

  key = ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_W, ui::EF_NONE);
  dispatcher.ProcessEvent(mojom::Event::From(key));
  EXPECT_EQ(0u, event_dispatcher_delegate.GetAndClearLastAccelerator());

  uint32_t accelerator_2 = 2;
  matcher = mus::CreateKeyMatcher(mus::mojom::KeyboardCode::W,
                                  mus::mojom::kEventFlagNone);
  dispatcher.AddAccelerator(accelerator_2, std::move(matcher));
  dispatcher.ProcessEvent(mojom::Event::From(key));
  EXPECT_EQ(accelerator_2,
            event_dispatcher_delegate.GetAndClearLastAccelerator());

  dispatcher.RemoveAccelerator(accelerator_2);
  dispatcher.ProcessEvent(mojom::Event::From(key));
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
  EnableHitTest(&child);

  TestEventDispatcherDelegate event_dispatcher_delegate(&root);
  EventDispatcher dispatcher(&event_dispatcher_delegate);
  dispatcher.set_root(&root);

  MouseEventTest tests[] = {
      // Send a mouse down event over child.
      {ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(20, 25),
                      gfx::Point(20, 25), base::TimeDelta(),
                      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON),
       &child, gfx::Point(20, 25), gfx::Point(10, 15), nullptr, gfx::Point(),
       gfx::Point()},

      // Capture should be activated. Let's send a mouse move outside the bounds
      // of the child.
      {ui::MouseEvent(ui::ET_MOUSE_MOVED, gfx::Point(50, 50),
                      gfx::Point(50, 50), base::TimeDelta(),
                      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON),
       &child, gfx::Point(50, 50), gfx::Point(40, 40), nullptr, gfx::Point(),
       gfx::Point()},
      // Release the mouse and verify that the mouse up event goes to the child.
      {ui::MouseEvent(ui::ET_MOUSE_RELEASED, gfx::Point(50, 50),
                      gfx::Point(50, 50), base::TimeDelta(),
                      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON),
       &child, gfx::Point(50, 50), gfx::Point(40, 40), nullptr, gfx::Point(),
       gfx::Point()},

      // A mouse move at (50, 50) should now go to the root window. As the
      // move crosses between |child| and |root| |child| gets an exit, and
      // |root| the move.
      {ui::MouseEvent(ui::ET_MOUSE_MOVED, gfx::Point(50, 50),
                      gfx::Point(50, 50), base::TimeDelta(),
                      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON),
       &child, gfx::Point(50, 50), gfx::Point(40, 40), &root,
       gfx::Point(50, 50), gfx::Point(50, 50)},

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
  EnableHitTest(&child);

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
       &child, gfx::Point(20, 25), gfx::Point(10, 15), nullptr, gfx::Point(),
       gfx::Point()},

      // Capture should be activated. Let's send a mouse move outside the bounds
      // of the child and press the right mouse button too.
      {ui::MouseEvent(ui::ET_MOUSE_MOVED, gfx::Point(50, 50),
                      gfx::Point(50, 50), base::TimeDelta(),
                      ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON, 0),
       &child, gfx::Point(50, 50), gfx::Point(40, 40), nullptr, gfx::Point(),
       gfx::Point()},

      // Release the left mouse button and verify that the mouse up event goes
      // to the child.
      {ui::MouseEvent(ui::ET_MOUSE_RELEASED, gfx::Point(50, 50),
                      gfx::Point(50, 50), base::TimeDelta(),
                      ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON,
                      ui::EF_RIGHT_MOUSE_BUTTON),
       &child, gfx::Point(50, 50), gfx::Point(40, 40), nullptr, gfx::Point(),
       gfx::Point()},

      // A mouse move at (50, 50) should still go to the child.
      {ui::MouseEvent(ui::ET_MOUSE_MOVED, gfx::Point(50, 50),
                      gfx::Point(50, 50), base::TimeDelta(),
                      ui::EF_LEFT_MOUSE_BUTTON, 0),
       &child, gfx::Point(50, 50), gfx::Point(40, 40), nullptr, gfx::Point(),
       gfx::Point()},

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
  EnableHitTest(&child);

  root.SetBounds(gfx::Rect(0, 0, 100, 100));
  child.SetBounds(gfx::Rect(10, 10, 20, 20));

  child.SetClientArea(gfx::Insets(5, 5, 5, 5), std::vector<gfx::Rect>());

  TestEventDispatcherDelegate event_dispatcher_delegate(&root);
  EventDispatcher dispatcher(&event_dispatcher_delegate);
  dispatcher.set_root(&root);

  // Start move loop by sending mouse event over non-client area.
  const ui::MouseEvent press_event(
      ui::ET_MOUSE_PRESSED, gfx::Point(12, 12), gfx::Point(12, 12),
      base::TimeDelta(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  dispatcher.ProcessEvent(
      mojom::Event::From(static_cast<const ui::Event&>(press_event)));

  // Events should target child and be in the non-client area.
  scoped_ptr<DispatchedEventDetails> details =
      event_dispatcher_delegate.GetAndAdvanceDispatchedEventDetails();
  EXPECT_FALSE(event_dispatcher_delegate.has_queued_events());
  ASSERT_TRUE(details);
  ASSERT_EQ(&child, details->window);
  EXPECT_TRUE(details->in_nonclient_area);

  // Move the mouse 5,6 pixels and target is the same.
  const ui::MouseEvent move_event(ui::ET_MOUSE_MOVED, gfx::Point(17, 18),
                                  gfx::Point(17, 18), base::TimeDelta(),
                                  ui::EF_LEFT_MOUSE_BUTTON, 0);
  dispatcher.ProcessEvent(
      mojom::Event::From(static_cast<const ui::Event&>(move_event)));

  // Still same target.
  details = event_dispatcher_delegate.GetAndAdvanceDispatchedEventDetails();
  EXPECT_FALSE(event_dispatcher_delegate.has_queued_events());
  ASSERT_EQ(&child, details->window);
  EXPECT_TRUE(details->in_nonclient_area);

  // Release the mouse.
  const ui::MouseEvent release_event(
      ui::ET_MOUSE_RELEASED, gfx::Point(17, 18), gfx::Point(17, 18),
      base::TimeDelta(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  dispatcher.ProcessEvent(
      mojom::Event::From(static_cast<const ui::Event&>(release_event)));

  // The event should not have been dispatched to the delegate.
  details = event_dispatcher_delegate.GetAndAdvanceDispatchedEventDetails();
  EXPECT_FALSE(event_dispatcher_delegate.has_queued_events());
  ASSERT_EQ(&child, details->window);
  EXPECT_TRUE(details->in_nonclient_area);

  // Press in the client area and verify target/client area. The non-client area
  // should get an exit first.
  const ui::MouseEvent press_event2(
      ui::ET_MOUSE_PRESSED, gfx::Point(21, 22), gfx::Point(21, 22),
      base::TimeDelta(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  dispatcher.ProcessEvent(
      mojom::Event::From(static_cast<const ui::Event&>(press_event2)));
  details = event_dispatcher_delegate.GetAndAdvanceDispatchedEventDetails();
  EXPECT_TRUE(event_dispatcher_delegate.has_queued_events());
  ASSERT_EQ(&child, details->window);
  EXPECT_TRUE(details->in_nonclient_area);
  EXPECT_EQ(mojom::EventType::MOUSE_EXIT, details->event->action);

  details = event_dispatcher_delegate.GetAndAdvanceDispatchedEventDetails();
  EXPECT_FALSE(event_dispatcher_delegate.has_queued_events());
  ASSERT_EQ(&child, details->window);
  EXPECT_FALSE(details->in_nonclient_area);
  EXPECT_EQ(mojom::EventType::POINTER_DOWN, details->event->action);
}

TEST(EventDispatcherTest, AdditionalClientArea) {
  TestServerWindowDelegate window_delegate;
  ServerWindow root(&window_delegate, WindowId(1, 2));
  window_delegate.set_root_window(&root);
  root.SetVisible(true);

  ServerWindow child(&window_delegate, WindowId(1, 3));
  root.Add(&child);
  child.SetVisible(true);
  EnableHitTest(&child);

  root.SetBounds(gfx::Rect(0, 0, 100, 100));
  child.SetBounds(gfx::Rect(10, 10, 20, 20));

  std::vector<gfx::Rect> additional_client_areas;
  additional_client_areas.push_back(gfx::Rect(18, 0, 2, 2));
  child.SetClientArea(gfx::Insets(5, 5, 5, 5), additional_client_areas);

  TestEventDispatcherDelegate event_dispatcher_delegate(&root);
  EventDispatcher dispatcher(&event_dispatcher_delegate);
  dispatcher.set_root(&root);

  // Press in the additional client area, it should go to the child.
  const ui::MouseEvent press_event(
      ui::ET_MOUSE_PRESSED, gfx::Point(28, 11), gfx::Point(28, 11),
      base::TimeDelta(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  dispatcher.ProcessEvent(
      mojom::Event::From(static_cast<const ui::Event&>(press_event)));

  // Events should target child and be in the client area.
  scoped_ptr<DispatchedEventDetails> details =
      event_dispatcher_delegate.GetAndAdvanceDispatchedEventDetails();
  EXPECT_FALSE(event_dispatcher_delegate.has_queued_events());
  ASSERT_EQ(&child, details->window);
  EXPECT_FALSE(details->in_nonclient_area);
}

TEST(EventDispatcherTest, DontFocusOnSecondDown) {
  TestServerWindowDelegate window_delegate;
  ServerWindow root(&window_delegate, WindowId(1, 2));
  window_delegate.set_root_window(&root);
  root.SetVisible(true);

  ServerWindow child1(&window_delegate, WindowId(1, 3));
  root.Add(&child1);
  child1.SetVisible(true);
  EnableHitTest(&child1);

  ServerWindow child2(&window_delegate, WindowId(1, 4));
  root.Add(&child2);
  child2.SetVisible(true);
  EnableHitTest(&child2);

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
  dispatcher.ProcessEvent(
      mojom::Event::From(static_cast<const ui::Event&>(press_event)));
  scoped_ptr<DispatchedEventDetails> details =
      event_dispatcher_delegate.GetAndAdvanceDispatchedEventDetails();
  EXPECT_FALSE(event_dispatcher_delegate.has_queued_events());
  EXPECT_EQ(&child1, details->window);
  EXPECT_EQ(&child1, event_dispatcher_delegate.GetAndClearLastFocusedWindow());

  // Press (with a different pointer id) on child2. Event should go to child2,
  // but focus should not change.
  const ui::TouchEvent touch_event(ui::ET_TOUCH_PRESSED, gfx::Point(53, 54), 2,
                                   base::TimeDelta());
  dispatcher.ProcessEvent(
      mojom::Event::From(static_cast<const ui::Event&>(touch_event)));
  details = event_dispatcher_delegate.GetAndAdvanceDispatchedEventDetails();
  EXPECT_FALSE(event_dispatcher_delegate.has_queued_events());
  EXPECT_EQ(&child2, details->window);
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
  EnableHitTest(&child1);

  ServerWindow child2(&window_delegate, WindowId(1, 4));
  root.Add(&child2);
  child2.SetVisible(true);
  EnableHitTest(&child2);

  root.SetBounds(gfx::Rect(0, 0, 100, 100));
  child1.SetBounds(gfx::Rect(10, 10, 20, 20));
  child2.SetBounds(gfx::Rect(50, 51, 11, 12));

  TestEventDispatcherDelegate event_dispatcher_delegate(&root);
  EventDispatcher dispatcher(&event_dispatcher_delegate);
  dispatcher.set_root(&root);

  // Press on child1.
  const ui::TouchEvent touch_event1(ui::ET_TOUCH_PRESSED, gfx::Point(12, 13), 1,
                                    base::TimeDelta());
  dispatcher.ProcessEvent(
      mojom::Event::From(static_cast<const ui::Event&>(touch_event1)));
  scoped_ptr<DispatchedEventDetails> details =
      event_dispatcher_delegate.GetAndAdvanceDispatchedEventDetails();
  EXPECT_EQ(&child1, details->window);

  // Drag over child2, child1 should get the drag.
  const ui::TouchEvent drag_event1(ui::ET_TOUCH_MOVED, gfx::Point(53, 54), 1,
                                   base::TimeDelta());
  dispatcher.ProcessEvent(
      mojom::Event::From(static_cast<const ui::Event&>(drag_event1)));
  details = event_dispatcher_delegate.GetAndAdvanceDispatchedEventDetails();
  EXPECT_EQ(&child1, details->window);

  // Press on child2 with a different touch id.
  const ui::TouchEvent touch_event2(ui::ET_TOUCH_PRESSED, gfx::Point(54, 55), 2,
                                    base::TimeDelta());
  dispatcher.ProcessEvent(
      mojom::Event::From(static_cast<const ui::Event&>(touch_event2)));
  details = event_dispatcher_delegate.GetAndAdvanceDispatchedEventDetails();
  EXPECT_EQ(&child2, details->window);

  // Drag over child1 with id 2, child2 should continue to get the drag.
  const ui::TouchEvent drag_event2(ui::ET_TOUCH_MOVED, gfx::Point(13, 14), 2,
                                   base::TimeDelta());
  dispatcher.ProcessEvent(
      mojom::Event::From(static_cast<const ui::Event&>(drag_event2)));
  details = event_dispatcher_delegate.GetAndAdvanceDispatchedEventDetails();
  EXPECT_EQ(&child2, details->window);

  // Drag again with id 1, child1 should continue to get it.
  dispatcher.ProcessEvent(
      mojom::Event::From(static_cast<const ui::Event&>(drag_event1)));
  details = event_dispatcher_delegate.GetAndAdvanceDispatchedEventDetails();
  EXPECT_EQ(&child1, details->window);

  // Release touch id 1, and click on 2. 2 should get it.
  const ui::TouchEvent touch_release(ui::ET_TOUCH_RELEASED, gfx::Point(54, 55),
                                     1, base::TimeDelta());
  dispatcher.ProcessEvent(
      mojom::Event::From(static_cast<const ui::Event&>(touch_release)));
  details = event_dispatcher_delegate.GetAndAdvanceDispatchedEventDetails();
  EXPECT_EQ(&child1, details->window);
  const ui::TouchEvent touch_event3(ui::ET_TOUCH_PRESSED, gfx::Point(54, 55), 2,
                                    base::TimeDelta());
  dispatcher.ProcessEvent(
      mojom::Event::From(static_cast<const ui::Event&>(touch_event3)));
  details = event_dispatcher_delegate.GetAndAdvanceDispatchedEventDetails();
  EXPECT_EQ(&child2, details->window);
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
  EnableHitTest(child.get());

  root.SetBounds(gfx::Rect(0, 0, 100, 100));
  child->SetBounds(gfx::Rect(10, 10, 20, 20));

  TestEventDispatcherDelegate event_dispatcher_delegate(&root);
  EventDispatcher dispatcher(&event_dispatcher_delegate);
  dispatcher.set_root(&root);

  // Press on child.
  const ui::TouchEvent touch_event1(ui::ET_TOUCH_PRESSED, gfx::Point(12, 13), 1,
                                    base::TimeDelta());
  dispatcher.ProcessEvent(
      mojom::Event::From(static_cast<const ui::Event&>(touch_event1)));
  scoped_ptr<DispatchedEventDetails> details =
      event_dispatcher_delegate.GetAndAdvanceDispatchedEventDetails();
  EXPECT_FALSE(event_dispatcher_delegate.has_queued_events());
  EXPECT_EQ(child.get(), details->window);

  // Delete child, and continue the drag. Event should not be dispatched.
  child.reset();

  const ui::TouchEvent drag_event1(ui::ET_TOUCH_MOVED, gfx::Point(53, 54), 1,
                                   base::TimeDelta());
  dispatcher.ProcessEvent(
      mojom::Event::From(static_cast<const ui::Event&>(drag_event1)));
  details = event_dispatcher_delegate.GetAndAdvanceDispatchedEventDetails();
  EXPECT_EQ(nullptr, details.get());
}

TEST(EventDispatcherTest, MouseInExtendedHitTestRegion) {
  TestServerWindowDelegate window_delegate;
  ServerWindow root(&window_delegate, WindowId(1, 2));
  window_delegate.set_root_window(&root);
  root.SetVisible(true);

  ServerWindow child(&window_delegate, WindowId(1, 3));
  root.Add(&child);
  child.SetVisible(true);
  EnableHitTest(&child);

  root.SetBounds(gfx::Rect(0, 0, 100, 100));
  child.SetBounds(gfx::Rect(10, 10, 20, 20));

  TestEventDispatcherDelegate event_dispatcher_delegate(&root);
  EventDispatcher dispatcher(&event_dispatcher_delegate);
  dispatcher.set_root(&root);

  // Send event that is not over child.
  const ui::MouseEvent ui_event(
      ui::ET_MOUSE_PRESSED, gfx::Point(8, 9), gfx::Point(8, 9),
      base::TimeDelta(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  dispatcher.ProcessEvent(
      mojom::Event::From(static_cast<const ui::Event&>(ui_event)));
  scoped_ptr<DispatchedEventDetails> details =
      event_dispatcher_delegate.GetAndAdvanceDispatchedEventDetails();
  ASSERT_EQ(&root, details->window);

  // Release the mouse.
  const ui::MouseEvent release_event(
      ui::ET_MOUSE_RELEASED, gfx::Point(8, 9), gfx::Point(8, 9),
      base::TimeDelta(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  dispatcher.ProcessEvent(
      mojom::Event::From(static_cast<const ui::Event&>(release_event)));
  details = event_dispatcher_delegate.GetAndAdvanceDispatchedEventDetails();
  EXPECT_FALSE(event_dispatcher_delegate.has_queued_events());
  ASSERT_EQ(&root, details->window);
  EXPECT_FALSE(details->in_nonclient_area);

  // Change the extended hit test region and send event in extended hit test
  // region. Should result in exit for root, followed by press for child.
  child.set_extended_hit_test_region(gfx::Insets(5, 5, 5, 5));
  dispatcher.ProcessEvent(
      mojom::Event::From(static_cast<const ui::Event&>(ui_event)));
  details = event_dispatcher_delegate.GetAndAdvanceDispatchedEventDetails();
  EXPECT_EQ(&root, details->window);
  EXPECT_EQ(mojom::EventType::MOUSE_EXIT, details->event->action);
  details = event_dispatcher_delegate.GetAndAdvanceDispatchedEventDetails();
  ASSERT_TRUE(details);

  EXPECT_FALSE(event_dispatcher_delegate.has_queued_events());
  EXPECT_TRUE(details->in_nonclient_area);
  ASSERT_EQ(&child, details->window);
  EXPECT_EQ(mojom::EventType::POINTER_DOWN, details->event->action);
  scoped_ptr<ui::Event> dispatched_event(
      details->event.To<scoped_ptr<ui::Event>>());
  ASSERT_TRUE(dispatched_event.get());
  ASSERT_TRUE(dispatched_event->IsMouseEvent());
  ui::MouseEvent* dispatched_mouse_event =
      static_cast<ui::MouseEvent*>(dispatched_event.get());
  EXPECT_EQ(gfx::Point(-2, -1), dispatched_mouse_event->location());
}

TEST(EventDispatcherTest, WheelWhileDown) {
  TestServerWindowDelegate window_delegate;
  ServerWindow root(&window_delegate, WindowId(1, 2));
  window_delegate.set_root_window(&root);
  root.SetVisible(true);

  ServerWindow child1(&window_delegate, WindowId(1, 3));
  root.Add(&child1);
  child1.SetVisible(true);
  EnableHitTest(&child1);

  ServerWindow child2(&window_delegate, WindowId(1, 4));
  root.Add(&child2);
  child2.SetVisible(true);
  EnableHitTest(&child2);

  root.SetBounds(gfx::Rect(0, 0, 100, 100));
  child1.SetBounds(gfx::Rect(10, 10, 20, 20));
  child2.SetBounds(gfx::Rect(50, 51, 11, 12));

  TestEventDispatcherDelegate event_dispatcher_delegate(&root);
  EventDispatcher dispatcher(&event_dispatcher_delegate);
  dispatcher.set_root(&root);

  MouseEventTest tests[] = {
      // Send a mouse down event over child1.
      {ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(15, 15),
                      gfx::Point(15, 15), base::TimeDelta(),
                      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON),
       &child1, gfx::Point(15, 15), gfx::Point(5, 5), nullptr, gfx::Point(),
       gfx::Point()},
      // Send mouse wheel over child2, should go to child1 as it has capture.
      {ui::MouseWheelEvent(gfx::Vector2d(1, 0), gfx::Point(53, 54),
                           gfx::Point(53, 54), base::TimeDelta(), ui::EF_NONE,
                           ui::EF_NONE),
       &child1, gfx::Point(53, 54), gfx::Point(43, 44), nullptr, gfx::Point(),
       gfx::Point()},
  };
  RunMouseEventTests(&dispatcher, &event_dispatcher_delegate, tests,
                     arraysize(tests));
}

}  // namespace ws
}  // namespace mus
