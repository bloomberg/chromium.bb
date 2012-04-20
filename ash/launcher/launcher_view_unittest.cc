// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher_view.h"

#include "ash/ash_switches.h"
#include "ash/launcher/launcher.h"
#include "ash/launcher/launcher_icon_observer.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_launcher_delegate.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

namespace {

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
    count_++;
  }

  int count() const { return count_; }
  void Reset() { count_ = 0; }

 private:
  int count_;

  DISALLOW_COPY_AND_ASSIGN(TestLauncherIconObserver);
};

class LauncherViewTest : public ash::test::AshTestBase {
 public:
  LauncherViewTest() {}
  virtual ~LauncherViewTest() {}

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

  DISALLOW_COPY_AND_ASSIGN(LauncherViewTest);
};

TEST_F(LauncherViewTest, AddRemove) {
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

TEST_F(LauncherViewTest, BoundsChanged) {
  Launcher* launcher = Shell::GetInstance()->launcher();
  int total_width = launcher->widget()->GetWindowScreenBounds().width();
  ASSERT_GT(total_width, 0);
  launcher->SetStatusWidth(total_width / 2);
  EXPECT_EQ(1, observer()->count());
  observer()->Reset();
}

}  // namespace

}  // namespace ash
