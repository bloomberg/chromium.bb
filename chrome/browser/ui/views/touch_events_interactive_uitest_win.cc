// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/win/windows_version.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/view_event_test_base.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/test/ui_controls.h"
#include "ui/views/controls/menu/menu_runner.h"

namespace {

class TouchEventHandler : public ui::EventHandler {
 public:
  TouchEventHandler()
      : call_depth_(0),
        max_call_depth_(0),
        num_touch_presses_(0),
        num_pointers_down_(0),
        recursion_enabled_(false) {}

  ~TouchEventHandler() override {}

  // OnTouchEvent will simulate a second touch event (at |touch_point|) to force
  // recursion in event handling.
  void ForceRecursionInEventHandler(const gfx::Point& touch_point) {
    recursion_enabled_ = true;
    touch_point_ = touch_point;
  }

  void WaitForIdle() {
    base::MessageLoopForUI* loop = base::MessageLoopForUI::current();
    base::MessageLoopForUI::ScopedNestableTaskAllower allow_nested(loop);
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }
  void WaitForEvents() {
    base::MessageLoopForUI* loop = base::MessageLoopForUI::current();
    base::MessageLoopForUI::ScopedNestableTaskAllower allow_nested(loop);
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  int max_call_depth() const { return max_call_depth_; }

  int num_touch_presses() const { return num_touch_presses_; }

  int num_pointers_down() const { return num_pointers_down_; }

  void set_touch_point(const gfx::Point& pt) { touch_point_ = pt; }

 private:
  // ui::EventHandler:
  void OnTouchEvent(ui::TouchEvent* event) override {
    max_call_depth_ = std::max(++call_depth_, max_call_depth_);

    if (recursion_enabled_ && (event->type() == ui::ET_TOUCH_RELEASED)) {
      recursion_enabled_ = false;
      ui_controls::SendTouchEvents(ui_controls::PRESS | ui_controls::PRESS, 1,
                                   touch_point_.x(), touch_point_.y());
      WaitForIdle();
    }

    switch (event->type()) {
      case ui::ET_TOUCH_PRESSED:
        num_touch_presses_++;
        num_pointers_down_++;
        break;
      case ui::ET_TOUCH_RELEASED:
        num_pointers_down_--;
        if (!quit_closure_.is_null() && num_pointers_down_ == 0) {
          quit_closure_.Run();
        }
        break;
      default:
        break;
    }
    --call_depth_;
  }

  int call_depth_;
  int max_call_depth_;
  int num_touch_presses_;
  int num_pointers_down_;
  base::Closure quit_closure_;
  bool recursion_enabled_;
  gfx::Point touch_point_;
  DISALLOW_COPY_AND_ASSIGN(TouchEventHandler);
};

}  // namespace

class TouchEventsViewTest : public ViewEventTestBase {
 public:
  TouchEventsViewTest() : ViewEventTestBase(), touch_view_(nullptr) {}

  // ViewEventTestBase:
  void SetUp() override {
    touch_view_ = new views::View();
    ViewEventTestBase::SetUp();
  }

  void TearDown() override {
    touch_view_ = nullptr;
    ViewEventTestBase::TearDown();
  }

  views::View* CreateContentsView() override { return touch_view_; }

  gfx::Size GetPreferredSizeForContents() const override {
    return gfx::Size(600, 600);
  }

  void DoTestOnMessageLoop() override {
    // ui_controls::SendTouchEvents which uses InjectTouchInput API only works
    // on Windows 8 and up.
    if (base::win::GetVersion() <= base::win::VERSION_WIN7) {
      Done();
      return;
    }

    const int touch_pointer_count = 3;
    TouchEventHandler touch_event_handler;
    GetWidget()->GetNativeWindow()->GetHost()->window()->AddPreTargetHandler(
        &touch_event_handler);
    gfx::Point in_content(touch_view_->width() / 2, touch_view_->height() / 2);
    views::View::ConvertPointToScreen(touch_view_, &in_content);

    ASSERT_TRUE(ui_controls::SendTouchEvents(ui_controls::PRESS,
                                             touch_pointer_count,
                                             in_content.x(), in_content.y()));
    touch_event_handler.WaitForIdle();
    touch_event_handler.WaitForEvents();
    EXPECT_EQ(touch_pointer_count, touch_event_handler.num_touch_presses());
    EXPECT_EQ(0, touch_event_handler.num_pointers_down());

    GetWidget()->GetNativeWindow()->GetHost()->window()->RemovePreTargetHandler(
        &touch_event_handler);
    Done();
  }

 protected:
  views::View* touch_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TouchEventsViewTest);
};

VIEW_TEST(TouchEventsViewTest, CheckWindowsNativeMessageForTouchEvents)

class TouchEventsRecursiveViewTest : public TouchEventsViewTest {
 public:
  TouchEventsRecursiveViewTest() {}

  void DoTestOnMessageLoop() override {
    // ui_controls::SendTouchEvents which uses InjectTouchInput API only works
    // on Windows 8 and up.
    if (base::win::GetVersion() <= base::win::VERSION_WIN7) {
      Done();
      return;
    }

    const int touch_pointer_count = 1;
    TouchEventHandler touch_event_handler;
    GetWidget()->GetNativeWindow()->GetHost()->window()->AddPreTargetHandler(
        &touch_event_handler);
    gfx::Point in_content(touch_view_->width() / 2, touch_view_->height() / 2);
    views::View::ConvertPointToScreen(touch_view_, &in_content);
    touch_event_handler.ForceRecursionInEventHandler(in_content);

    ASSERT_TRUE(ui_controls::SendTouchEvents(ui_controls::PRESS,
                                             touch_pointer_count,
                                             in_content.x(), in_content.y()));
    touch_event_handler.WaitForEvents();

    EXPECT_EQ(touch_pointer_count + 1, touch_event_handler.num_touch_presses());
    EXPECT_EQ(0, touch_event_handler.num_pointers_down());
    EXPECT_EQ(2, touch_event_handler.max_call_depth());
    GetWidget()->GetNativeWindow()->GetHost()->window()->RemovePreTargetHandler(
        &touch_event_handler);
    Done();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TouchEventsRecursiveViewTest);
};

VIEW_TEST(TouchEventsRecursiveViewTest, CheckWindowsRecursiveHandler)
