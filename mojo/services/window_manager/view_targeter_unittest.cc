// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/window_manager/view_targeter.h"

#include "mojo/services/window_manager/basic_focus_rules.h"
#include "mojo/services/window_manager/capture_controller.h"
#include "mojo/services/window_manager/focus_controller.h"
#include "mojo/services/window_manager/view_event_dispatcher.h"
#include "mojo/services/window_manager/window_manager_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/test_event_handler.h"

namespace window_manager {

class ViewTargeterTest : public testing::Test {
 public:
  ViewTargeterTest() {}
  ~ViewTargeterTest() override {}

  void SetUp() override {
    view_event_dispatcher_.reset(new ViewEventDispatcher());
  }

  void TearDown() override {
    view_event_dispatcher_.reset();
    testing::Test::TearDown();
  }

 protected:
  scoped_ptr<ViewEventDispatcher> view_event_dispatcher_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ViewTargeterTest);
};

TEST_F(ViewTargeterTest, Basic) {
  // The dispatcher will take ownership of the tree root.
  TestView root(1, gfx::Rect(0, 0, 100, 100));
  ViewTarget* root_target = root.target();
  root_target->SetEventTargeter(scoped_ptr<ViewTargeter>(new ViewTargeter()));
  view_event_dispatcher_->SetRootViewTarget(root_target);

  CaptureController capture_controller;
  SetCaptureController(&root, &capture_controller);

  TestView one(2, gfx::Rect(0, 0, 500, 100));
  TestView two(3, gfx::Rect(501, 0, 500, 1000));

  root.AddChild(&one);
  root.AddChild(&two);

  ui::test::TestEventHandler handler;
  one.target()->AddPreTargetHandler(&handler);

  ui::MouseEvent press(ui::ET_MOUSE_PRESSED, gfx::Point(20, 20),
                       gfx::Point(20, 20), ui::EventTimeForNow(), ui::EF_NONE,
                       ui::EF_NONE);
  ui::EventDispatchDetails details =
      view_event_dispatcher_->OnEventFromSource(&press);
  ASSERT_FALSE(details.dispatcher_destroyed);

  EXPECT_EQ(1, handler.num_mouse_events());

  one.target()->RemovePreTargetHandler(&handler);
}

TEST_F(ViewTargeterTest, KeyTest) {
  // The dispatcher will take ownership of the tree root.
  TestView root(1, gfx::Rect(0, 0, 100, 100));
  ViewTarget* root_target = root.target();
  root_target->SetEventTargeter(scoped_ptr<ViewTargeter>(new ViewTargeter()));
  view_event_dispatcher_->SetRootViewTarget(root_target);

  CaptureController capture_controller;
  SetCaptureController(&root, &capture_controller);

  TestView one(2, gfx::Rect(0, 0, 500, 100));
  TestView two(3, gfx::Rect(501, 0, 500, 1000));

  root.AddChild(&one);
  root.AddChild(&two);

  ui::test::TestEventHandler one_handler;
  one.target()->AddPreTargetHandler(&one_handler);

  ui::test::TestEventHandler two_handler;
  two.target()->AddPreTargetHandler(&two_handler);

  FocusController focus_controller(make_scoped_ptr(new BasicFocusRules(&root)));
  SetFocusController(&root, &focus_controller);

  // Focus |one|. Then test that it receives a key event.
  focus_controller.FocusView(&one);
  ui::KeyEvent key_event_one(ui::ET_KEY_PRESSED, ui::VKEY_A, 0);
  ui::EventDispatchDetails details =
      view_event_dispatcher_->OnEventFromSource(&key_event_one);
  ASSERT_FALSE(details.dispatcher_destroyed);
  EXPECT_EQ(1, one_handler.num_key_events());

  // Focus |two|. Then test that it receives a key event.
  focus_controller.FocusView(&two);
  ui::KeyEvent key_event_two(ui::ET_KEY_PRESSED, ui::VKEY_A, 0);
  details = view_event_dispatcher_->OnEventFromSource(&key_event_two);
  ASSERT_FALSE(details.dispatcher_destroyed);
  EXPECT_EQ(1, two_handler.num_key_events());

  two.target()->RemovePreTargetHandler(&two_handler);
  one.target()->RemovePreTargetHandler(&one_handler);
}

}  // namespace window_manager
