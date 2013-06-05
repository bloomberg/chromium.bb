// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/immersive_mode_controller_ash.h"

#include "ash/test/ash_test_base.h"
#include "chrome/browser/ui/immersive_fullscreen_configuration.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"

// For now, immersive fullscreen is Chrome OS only.
#if defined(OS_CHROMEOS)

/////////////////////////////////////////////////////////////////////////////

class MockImmersiveModeControllerDelegate
    : public ImmersiveModeController::Delegate {
 public:
  MockImmersiveModeControllerDelegate() : immersive_style_(false) {}
  virtual ~MockImmersiveModeControllerDelegate() {}

  bool immersive_style() const { return immersive_style_; }

  // ImmersiveModeController::Delegate overrides:
  virtual BookmarkBarView* GetBookmarkBar() OVERRIDE { return NULL; }
  virtual FullscreenController* GetFullscreenController() OVERRIDE {
    return NULL;
  }
  virtual void FocusLocationBar() OVERRIDE {}
  virtual void FullscreenStateChanged() OVERRIDE {}
  virtual void SetImmersiveStyle(bool immersive) OVERRIDE {
    immersive_style_ = immersive;
  }

 private:
  bool immersive_style_;

  DISALLOW_COPY_AND_ASSIGN(MockImmersiveModeControllerDelegate);
};

/////////////////////////////////////////////////////////////////////////////

class ImmersiveModeControllerAshTest : public ash::test::AshTestBase {
 public:
  ImmersiveModeControllerAshTest() : widget_(NULL), top_container_(NULL) {}
  virtual ~ImmersiveModeControllerAshTest() {}

  ImmersiveModeControllerAsh* controller() { return controller_.get(); }
  MockImmersiveModeControllerDelegate* delegate() { return delegate_.get(); }

  // ash::test::AshTestBase overrides:
  virtual void SetUp() OVERRIDE {
    ash::test::AshTestBase::SetUp();

    ImmersiveFullscreenConfiguration::EnableImmersiveFullscreenForTest();
    ASSERT_TRUE(ImmersiveFullscreenConfiguration::UseImmersiveFullscreen());

    controller_.reset(new ImmersiveModeControllerAsh);
    delegate_.reset(new MockImmersiveModeControllerDelegate);

    widget_ = new views::Widget();
    views::Widget::InitParams params;
    params.context = CurrentContext();
    widget_->Init(params);
    widget_->Show();

    top_container_ = new TopContainerView(NULL);
    widget_->GetContentsView()->AddChildView(top_container_);

    controller_->Init(delegate_.get(), widget_, top_container_);
    controller_->DisableAnimationsForTest();
  }

 private:
  scoped_ptr<ImmersiveModeControllerAsh> controller_;
  scoped_ptr<MockImmersiveModeControllerDelegate> delegate_;
  views::Widget* widget_;  // Owned by the native widget.
  TopContainerView* top_container_;  // Owned by |root_view_|.
  DISALLOW_COPY_AND_ASSIGN(ImmersiveModeControllerAshTest);
};

// Test of initial state and basic functionality.
TEST_F(ImmersiveModeControllerAshTest, ImmersiveModeControllerAsh) {
  // Initial state.
  EXPECT_FALSE(controller()->IsEnabled());
  EXPECT_FALSE(controller()->ShouldHideTopViews());
  EXPECT_FALSE(controller()->IsRevealed());
  EXPECT_FALSE(delegate()->immersive_style());

  // Enabling hides the top views.
  controller()->SetEnabled(true);
  EXPECT_TRUE(controller()->IsEnabled());
  EXPECT_FALSE(controller()->IsRevealed());
  EXPECT_TRUE(controller()->ShouldHideTopViews());
  EXPECT_FALSE(controller()->ShouldHideTabIndicators());
  EXPECT_TRUE(delegate()->immersive_style());

  // Revealing shows the top views.
  controller()->StartRevealForTest(true);
  EXPECT_TRUE(controller()->IsRevealed());
  EXPECT_FALSE(controller()->ShouldHideTopViews());
  // Tabs are painting in the normal style during a reveal.
  EXPECT_FALSE(delegate()->immersive_style());
}

#endif  // defined(OS_CHROMEOS)
