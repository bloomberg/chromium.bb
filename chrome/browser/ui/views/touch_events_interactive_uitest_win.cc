// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  TouchEventHandler() : num_touch_presses_(0), num_pointers_down_(0) {}

  ~TouchEventHandler() override {}

  void WaitForEvents() {
    base::MessageLoopForUI* loop = base::MessageLoopForUI::current();
    base::MessageLoopForUI::ScopedNestableTaskAllower allow_nested(loop);
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  int num_touch_presses() const { return num_touch_presses_; }

  int num_pointers_down() const { return num_pointers_down_; }

 private:
  // ui::EventHandler:
  void OnTouchEvent(ui::TouchEvent* event) override {
    switch (event->type()) {
      case ui::ET_TOUCH_PRESSED:
        num_touch_presses_++;
        num_pointers_down_++;
        break;
      case ui::ET_TOUCH_RELEASED:
        num_pointers_down_--;
        if (!quit_closure_.is_null() && num_pointers_down_ == 0)
          quit_closure_.Run();
        break;
      default:
        break;
    }
  }

  int num_touch_presses_;
  int num_pointers_down_;
  base::Closure quit_closure_;
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
    touch_event_handler.WaitForEvents();
    EXPECT_EQ(touch_pointer_count, touch_event_handler.num_touch_presses());
    EXPECT_EQ(0, touch_event_handler.num_pointers_down());

    GetWidget()->GetNativeWindow()->GetHost()->window()->RemovePreTargetHandler(
        &touch_event_handler);
    Done();
  }

 private:
  views::View* touch_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TouchEventsViewTest);
};

VIEW_TEST(TouchEventsViewTest, CheckWindowsNativeMessageForTouchEvents)
