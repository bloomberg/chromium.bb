// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/first_run/first_run_helper.h"

#include "ash/first_run/desktop_cleaner.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ui/events/event_handler.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace ash {
namespace test {

namespace {

class TestModalDialogDelegate : public views::DialogDelegateView {
 public:
  TestModalDialogDelegate() {}
  virtual ~TestModalDialogDelegate() {}

  // Overridden from views::WidgetDelegate:
  virtual ui::ModalType GetModalType() const OVERRIDE {
    return ui::MODAL_TYPE_SYSTEM;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestModalDialogDelegate);
};

class CountingEventHandler : public ui::EventHandler {
 public:
  // Handler resets |*mouse_events_registered_| during construction and updates
  // it after each registered event.
  explicit CountingEventHandler(int* mouse_events_registered)
      : mouse_events_registered_(mouse_events_registered) {
    *mouse_events_registered = 0;
  }

  virtual ~CountingEventHandler() {}

 private:
  // ui::EventHandler overrides.
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE {
    ++*mouse_events_registered_;
  }

  int* mouse_events_registered_;

  DISALLOW_COPY_AND_ASSIGN(CountingEventHandler);
};

}  // namespace

class FirstRunHelperTest : public AshTestBase,
                           public FirstRunHelper::Observer {
 public:
  FirstRunHelperTest() : cancelled_times_(0) {}

  virtual ~FirstRunHelperTest() {}

  virtual void SetUp() OVERRIDE {
    AshTestBase::SetUp();
    CheckContainersAreVisible();
    helper_.reset(ash::Shell::GetInstance()->CreateFirstRunHelper());
    helper_->AddObserver(this);
    helper_->GetOverlayWidget()->Show();
  }

  virtual void TearDown() OVERRIDE {
    EXPECT_TRUE(helper_.get());
    helper_.reset();
    CheckContainersAreVisible();
    AshTestBase::TearDown();
  }

  void CheckContainersAreVisible() const {
    aura::Window* root_window = Shell::GetInstance()->GetPrimaryRootWindow();
    std::vector<int> containers_to_check =
        DesktopCleaner::GetContainersToHideForTest();
    for (size_t i = 0; i < containers_to_check.size(); ++i) {
      aura::Window* container =
          Shell::GetContainer(root_window, containers_to_check[i]);
      EXPECT_TRUE(container->IsVisible());
    }
  }

  void CheckContainersAreHidden() const {
    aura::Window* root_window = Shell::GetInstance()->GetPrimaryRootWindow();
    std::vector<int> containers_to_check =
        DesktopCleaner::GetContainersToHideForTest();
    for (size_t i = 0; i < containers_to_check.size(); ++i) {
      aura::Window* container =
          Shell::GetContainer(root_window, containers_to_check[i]);
      EXPECT_TRUE(!container->IsVisible());
    }
  }

  FirstRunHelper* helper() { return helper_.get(); }

  int cancelled_times() const { return cancelled_times_; }

 private:
  // FirstRunHelper::Observer overrides.
  virtual void OnCancelled() OVERRIDE {
    ++cancelled_times_;
  }

  scoped_ptr<FirstRunHelper> helper_;
  int cancelled_times_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunHelperTest);
};

// This test creates helper, checks that containers are hidden and then
// destructs helper.
TEST_F(FirstRunHelperTest, ContainersAreHidden) {
  CheckContainersAreHidden();
}

// Tests that helper correctly handles Escape key press.
TEST_F(FirstRunHelperTest, Cancel) {
  GetEventGenerator().PressKey(ui::VKEY_ESCAPE, 0);
  EXPECT_EQ(cancelled_times(), 1);
}

// Tests that modal window doesn't block events for overlay window.
TEST_F(FirstRunHelperTest, ModalWindowDoesNotBlock) {
  views::Widget* modal_dialog = views::DialogDelegate::CreateDialogWidget(
      new TestModalDialogDelegate(), CurrentContext(), NULL);
  modal_dialog->Show();
  // TODO(dzhioev): modal window should not steal focus from overlay window.
  aura::Window* overlay_window = helper()->GetOverlayWidget()->GetNativeView();
  overlay_window->Focus();
  EXPECT_TRUE(overlay_window->HasFocus());
  int mouse_events;
  CountingEventHandler handler(&mouse_events);
  overlay_window->AddPreTargetHandler(&handler);
  GetEventGenerator().PressLeftButton();
  GetEventGenerator().ReleaseLeftButton();
  EXPECT_EQ(mouse_events, 2);
  overlay_window->RemovePreTargetHandler(&handler);
}

}  // namespace test
}  // namespace ash
