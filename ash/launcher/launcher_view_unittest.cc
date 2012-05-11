// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher_view.h"

#include "ash/launcher/launcher.h"
#include "ash/launcher/launcher_button.h"
#include "ash/launcher/launcher_icon_observer.h"
#include "ash/launcher/launcher_model.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/launcher_view_test_api.h"
#include "ash/test/test_launcher_delegate.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace test {

////////////////////////////////////////////////////////////////////////////////
// LauncherIconObserver tests.

class TestLauncherIconObserver : public LauncherIconObserver {
 public:
  TestLauncherIconObserver() : count_(0) {
    Shell::GetInstance()->launcher()->AddIconObserver(this);
  }

  virtual ~TestLauncherIconObserver() {
    Shell::GetInstance()->launcher()->RemoveIconObserver(this);
  }

  // LauncherIconObserver implementation.
  void OnLauncherIconPositionsChanged() OVERRIDE {
    ++count_;
  }

  int count() const { return count_; }
  void Reset() { count_ = 0; }

 private:
  int count_;

  DISALLOW_COPY_AND_ASSIGN(TestLauncherIconObserver);
};

class LauncherViewIconObserverTest : public ash::test::AshTestBase {
 public:
  LauncherViewIconObserverTest() {}
  virtual ~LauncherViewIconObserverTest() {}

  virtual void SetUp() OVERRIDE {
    AshTestBase::SetUp();
    observer_.reset(new TestLauncherIconObserver);
  }

  virtual void TearDown() OVERRIDE {
    observer_.reset();
    AshTestBase::TearDown();
  }

  TestLauncherIconObserver* observer() { return observer_.get(); }

 private:
  scoped_ptr<TestLauncherIconObserver> observer_;

  DISALLOW_COPY_AND_ASSIGN(LauncherViewIconObserverTest);
};

TEST_F(LauncherViewIconObserverTest, AddRemove) {
  ash::test::TestLauncherDelegate* launcher_delegate =
      ash::test::TestLauncherDelegate::instance();
  ASSERT_TRUE(launcher_delegate);

  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(0, 0, 200, 200);

  scoped_ptr<views::Widget> widget(new views::Widget());
  widget->Init(params);
  launcher_delegate->AddLauncherItem(widget->GetNativeWindow());
  EXPECT_EQ(1, observer()->count());
  observer()->Reset();

  widget->Show();
  widget->GetNativeWindow()->parent()->RemoveChild(widget->GetNativeWindow());
  EXPECT_EQ(1, observer()->count());
  observer()->Reset();
}

TEST_F(LauncherViewIconObserverTest, BoundsChanged) {
  Launcher* launcher = Shell::GetInstance()->launcher();
  gfx::Size launcher_size = launcher->widget()->GetWindowScreenBounds().size();
  int total_width = launcher_size.width() / 2;
  ASSERT_GT(total_width, 0);
  launcher->SetStatusSize(gfx::Size(total_width, launcher_size.height()));
  EXPECT_EQ(1, observer()->count());
  observer()->Reset();
}

////////////////////////////////////////////////////////////////////////////////
// LauncherView tests.

class LauncherViewTest : public aura::test::AuraTestBase {
 public:
  LauncherViewTest() {}
  virtual ~LauncherViewTest() {}

  virtual void SetUp() OVERRIDE {
    aura::test::AuraTestBase::SetUp();

    model_.reset(new LauncherModel);

    launcher_view_.reset(new internal::LauncherView(model_.get(), NULL));
    launcher_view_->Init();
    // The bounds should be big enough for 4 buttons + overflow chevron.
    launcher_view_->SetBounds(0, 0, 500, 50);

    test_api_.reset(new LauncherViewTestAPI(launcher_view_.get()));
    test_api_->SetAnimationDuration(1);  // Speeds up animation for test.
  }

 protected:
  LauncherID AddAppShortcut() {
    LauncherItem item;
    item.type = TYPE_APP_SHORTCUT;
    item.status = STATUS_CLOSED;

    LauncherID id = model_->next_id();
    model_->Add(item);
    test_api_->RunMessageLoopUntilAnimationsDone();
    return id;
  }

  LauncherID AddTabbedBrowserNoWait() {
    LauncherItem item;
    item.type = TYPE_TABBED;
    item.status = STATUS_RUNNING;

    LauncherID id = model_->next_id();
    model_->Add(item);
    return id;
  }

  LauncherID AddTabbedBrowser() {
    LauncherID id = AddTabbedBrowserNoWait();
    test_api_->RunMessageLoopUntilAnimationsDone();
    return id;
  }

  void RemoveByID(LauncherID id) {
    model_->RemoveItemAt(model_->ItemIndexByID(id));
    test_api_->RunMessageLoopUntilAnimationsDone();
  }

  internal::LauncherButton* GetButtonByID(LauncherID id) {
    int index = model_->ItemIndexByID(id);
    return test_api_->GetButton(index);
  }

  scoped_ptr<LauncherModel> model_;
  scoped_ptr<internal::LauncherView> launcher_view_;
  scoped_ptr<LauncherViewTestAPI> test_api_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LauncherViewTest);
};

// Adds browser button until overflow and verifies that the last added browser
// button is hidden.
TEST_F(LauncherViewTest, AddBrowserUntilOverflow) {
  // All buttons should be visible.
  ASSERT_EQ(test_api_->GetLastVisibleIndex() + 1,
            test_api_->GetButtonCount());

  // Add tabbed browser until overflow.
  LauncherID last_added = AddTabbedBrowser();
  while (!test_api_->IsOverflowButtonVisible()) {
    // Added button is visible after animation while in this loop.
    EXPECT_TRUE(GetButtonByID(last_added)->visible());

    last_added = AddTabbedBrowser();
  }

  // The last added button should be invisible.
  EXPECT_FALSE(GetButtonByID(last_added)->visible());
}

// Adds one browser button then adds app shortcut until overflow. Verifies that
// the browser button gets hidden on overflow and last added app shortcut is
// still visible.
TEST_F(LauncherViewTest, AddAppShortcutWithBrowserButtonUntilOverflow) {
  // All buttons should be visible.
  ASSERT_EQ(test_api_->GetLastVisibleIndex() + 1,
            test_api_->GetButtonCount());

  LauncherID browser_button_id = AddTabbedBrowser();

  // Add app shortcut until overflow.
  LauncherID last_added = AddAppShortcut();
  while (!test_api_->IsOverflowButtonVisible()) {
    // Added button is visible after animation while in this loop.
    EXPECT_TRUE(GetButtonByID(last_added)->visible());

    last_added = AddAppShortcut();
  }

  // The last added app short button should be visible.
  EXPECT_TRUE(GetButtonByID(last_added)->visible());
  // And the browser button is invisible.
  EXPECT_FALSE(GetButtonByID(browser_button_id)->visible());
}

// Adds button until overflow then removes first added one. Verifies that
// the last added one changes from invisible to visible and overflow
// chevron is gone.
TEST_F(LauncherViewTest, RemoveButtonRevealsOverflowed) {
  // All buttons should be visible.
  ASSERT_EQ(test_api_->GetLastVisibleIndex() + 1,
            test_api_->GetButtonCount());

  // Add tabbed browser until overflow.
  LauncherID first_added= AddTabbedBrowser();
  LauncherID last_added = first_added;
  while (!test_api_->IsOverflowButtonVisible())
    last_added = AddTabbedBrowser();

  // Expect add more than 1 button. First added is visible and last is not.
  EXPECT_NE(first_added, last_added);
  EXPECT_TRUE(GetButtonByID(first_added)->visible());
  EXPECT_FALSE(GetButtonByID(last_added)->visible());

  // Remove first added.
  RemoveByID(first_added);

  // Last added button becomes visible and overflow chevron is gone.
  EXPECT_TRUE(GetButtonByID(last_added)->visible());
  EXPECT_EQ(1.0f, GetButtonByID(last_added)->layer()->opacity());
  EXPECT_FALSE(test_api_->IsOverflowButtonVisible());
}

// Verifies that remove last overflowed button should hide overflow chevron.
TEST_F(LauncherViewTest, RemoveLastOverflowed) {
  // All buttons should be visible.
  ASSERT_EQ(test_api_->GetLastVisibleIndex() + 1,
            test_api_->GetButtonCount());

  // Add tabbed browser until overflow.
  LauncherID last_added= AddTabbedBrowser();
  while (!test_api_->IsOverflowButtonVisible())
    last_added = AddTabbedBrowser();

  RemoveByID(last_added);
  EXPECT_FALSE(test_api_->IsOverflowButtonVisible());
}

// Adds browser button without waiting for animation to finish and verifies
// that all added buttons are visible.
TEST_F(LauncherViewTest, AddButtonQuickly) {
  // All buttons should be visible.
  ASSERT_EQ(test_api_->GetLastVisibleIndex() + 1,
            test_api_->GetButtonCount());

  // Add a few tabbed browser quickly without wait for animation.
  int added_count = 0;
  while (!test_api_->IsOverflowButtonVisible()) {
    AddTabbedBrowserNoWait();
    ++added_count;
  }

  // LauncherView should be big enough to hold at least 3 new buttons.
  ASSERT_GE(added_count, 3);

  // Wait for the last animation to finish.
  test_api_->RunMessageLoopUntilAnimationsDone();

  // Verifies non-overflow buttons are visible.
  for (int i = 0; i <= test_api_->GetLastVisibleIndex(); ++i) {
    internal::LauncherButton* button = test_api_->GetButton(i);
    EXPECT_TRUE(button->visible()) << "button index=" << i;
    EXPECT_EQ(1.0f, button->layer()->opacity()) << "button index=" << i;
  }
}

}  // namespace test
}  // namespace ash
