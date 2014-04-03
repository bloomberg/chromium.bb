// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/immersive_mode_controller_ash.h"

#include "ash/ash_switches.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_types.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/fullscreen/fullscreen_controller.h"
#include "chrome/browser/ui/fullscreen/fullscreen_controller_test.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "ui/aura/window.h"
#include "ui/views/controls/webview/webview.h"

class ImmersiveModeControllerAshTest : public TestWithBrowserView {
 public:
  ImmersiveModeControllerAshTest()
      : TestWithBrowserView(Browser::TYPE_TABBED,
                            chrome::HOST_DESKTOP_TYPE_ASH,
                            false) {
  }
  ImmersiveModeControllerAshTest(
      Browser::Type browser_type,
      chrome::HostDesktopType host_desktop_type,
      bool hosted_app)
      : TestWithBrowserView(browser_type,
                            host_desktop_type,
                            hosted_app) {
  }
  virtual ~ImmersiveModeControllerAshTest() {}

  // TestWithBrowserView override:
  virtual void SetUp() OVERRIDE {
    TestWithBrowserView::SetUp();

    browser()->window()->Show();

    controller_ = browser_view()->immersive_mode_controller();
    controller_->SetupForTest();
  }

  // Returns the bounds of |view| in widget coordinates.
  gfx::Rect GetBoundsInWidget(views::View* view) {
    return view->ConvertRectToWidget(view->GetLocalBounds());
  }

  // Toggle the browser's fullscreen state.
  void ToggleFullscreen() {
    // NOTIFICATION_FULLSCREEN_CHANGED is sent asynchronously. The notification
    // is used to trigger changes in whether the shelf is auto hidden and
    // whether a "light bar" version of the tab strip is used when the
    // top-of-window views are hidden.
    scoped_ptr<FullscreenNotificationObserver> waiter(
        new FullscreenNotificationObserver());
    chrome::ToggleFullscreenMode(browser());
    waiter->Wait();
  }

  // Set whether the browser is in tab fullscreen.
  void SetTabFullscreen(bool tab_fullscreen) {
    content::WebContents* web_contents =
        browser_view()->GetContentsWebViewForTest()->GetWebContents();
    scoped_ptr<FullscreenNotificationObserver> waiter(
        new FullscreenNotificationObserver());
    browser()->fullscreen_controller()->ToggleFullscreenModeForTab(
        web_contents, tab_fullscreen);
    waiter->Wait();
  }

  // Attempt revealing the top-of-window views.
  void AttemptReveal() {
    if (!revealed_lock_.get()) {
      revealed_lock_.reset(controller_->GetRevealedLock(
          ImmersiveModeControllerAsh::ANIMATE_REVEAL_NO));
    }
  }

  // Attempt unrevealing the top-of-window views.
  void AttemptUnreveal() {
    revealed_lock_.reset();
  }

  ImmersiveModeController* controller() { return controller_; }

 private:
  // Not owned.
  ImmersiveModeController* controller_;

  scoped_ptr<ImmersiveRevealedLock> revealed_lock_;

  DISALLOW_COPY_AND_ASSIGN(ImmersiveModeControllerAshTest);
};

// Test the layout and visibility of the tabstrip, toolbar and TopContainerView
// in immersive fullscreen.
TEST_F(ImmersiveModeControllerAshTest, Layout) {
  AddTab(browser(), GURL("about:blank"));

  TabStrip* tabstrip = browser_view()->tabstrip();
  ToolbarView* toolbar = browser_view()->toolbar();
  views::WebView* contents_web_view =
      browser_view()->GetContentsWebViewForTest();

  // Immersive fullscreen starts out disabled.
  ASSERT_FALSE(browser_view()->GetWidget()->IsFullscreen());
  ASSERT_FALSE(controller()->IsEnabled());

  // By default, the tabstrip and toolbar should be visible.
  EXPECT_TRUE(tabstrip->visible());
  EXPECT_TRUE(toolbar->visible());

  ToggleFullscreen();
  EXPECT_TRUE(browser_view()->GetWidget()->IsFullscreen());
  EXPECT_TRUE(controller()->IsEnabled());
  EXPECT_FALSE(controller()->IsRevealed());

  // Entering immersive fullscreen should make the tab strip use the immersive
  // style and hide the toolbar.
  EXPECT_TRUE(tabstrip->visible());
  EXPECT_TRUE(tabstrip->IsImmersiveStyle());
  EXPECT_FALSE(toolbar->visible());

  // The tab indicators should be flush with the top of the widget.
  EXPECT_EQ(0, GetBoundsInWidget(tabstrip).y());

  // The web contents should be immediately below the tab indicators.
  EXPECT_EQ(Tab::GetImmersiveHeight(),
            GetBoundsInWidget(contents_web_view).y());

  // Revealing the top-of-window views should set the tab strip back to the
  // normal style and show the toolbar.
  AttemptReveal();
  EXPECT_TRUE(controller()->IsRevealed());
  EXPECT_TRUE(tabstrip->visible());
  EXPECT_FALSE(tabstrip->IsImmersiveStyle());
  EXPECT_TRUE(toolbar->visible());

  // The TopContainerView should be flush with the top edge of the widget. If
  // it is not flush with the top edge the immersive reveal animation looks
  // wonky.
  EXPECT_EQ(0, GetBoundsInWidget(browser_view()->top_container()).y());

  // The web contents should be at the same y position as they were when the
  // top-of-window views were hidden.
  EXPECT_EQ(Tab::GetImmersiveHeight(),
            GetBoundsInWidget(contents_web_view).y());

  // Repeat the test for when in both immersive fullscreen and tab fullscreen.
  SetTabFullscreen(true);
  // Hide and reveal the top-of-window views so that they get relain out.
  AttemptUnreveal();
  AttemptReveal();

  // The tab strip and toolbar should still be visible and the TopContainerView
  // should still be flush with the top edge of the widget.
  EXPECT_TRUE(controller()->IsRevealed());
  EXPECT_TRUE(tabstrip->visible());
  EXPECT_FALSE(tabstrip->IsImmersiveStyle());
  EXPECT_TRUE(toolbar->visible());
  EXPECT_EQ(0, GetBoundsInWidget(browser_view()->top_container()).y());

  // The web contents should be flush with the top edge of the widget when in
  // both immersive and tab fullscreen.
  EXPECT_EQ(0, GetBoundsInWidget(contents_web_view).y());

  // Hide the top-of-window views. Both the tab strip and the toolbar should
  // hide when in both immersive and tab fullscreen.
  AttemptUnreveal();
  EXPECT_FALSE(controller()->IsRevealed());
  EXPECT_FALSE(tabstrip->visible());
  EXPECT_FALSE(toolbar->visible());

  // The web contents should still be flush with the edge of the widget.
  EXPECT_EQ(0, GetBoundsInWidget(contents_web_view).y());

  // Exiting both immersive and tab fullscreen should show the tab strip and
  // toolbar.
  ToggleFullscreen();
  EXPECT_FALSE(browser_view()->GetWidget()->IsFullscreen());
  EXPECT_FALSE(controller()->IsEnabled());
  EXPECT_FALSE(controller()->IsRevealed());
  EXPECT_TRUE(tabstrip->visible());
  EXPECT_FALSE(tabstrip->IsImmersiveStyle());
  EXPECT_TRUE(toolbar->visible());
}

// Test that the browser commands which are usually disabled in fullscreen are
// are enabled in immersive fullscreen.
TEST_F(ImmersiveModeControllerAshTest, EnabledCommands) {
  ASSERT_FALSE(controller()->IsEnabled());
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_OPEN_CURRENT_URL));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_ABOUT));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_LOCATION));

  ToggleFullscreen();
  EXPECT_TRUE(controller()->IsEnabled());
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_OPEN_CURRENT_URL));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_ABOUT));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_LOCATION));
}

// Test that restoring a window properly exits immersive fullscreen.
TEST_F(ImmersiveModeControllerAshTest, ExitUponRestore) {
  ASSERT_FALSE(controller()->IsEnabled());
  ToggleFullscreen();
  AttemptReveal();
  ASSERT_TRUE(controller()->IsEnabled());
  ASSERT_TRUE(controller()->IsRevealed());
  ASSERT_TRUE(browser_view()->GetWidget()->IsFullscreen());

  browser_view()->GetWidget()->Restore();
  EXPECT_FALSE(controller()->IsEnabled());
}

// Test how being simultaneously in tab fullscreen and immersive fullscreen
// affects the shelf visibility and whether the tab indicators are hidden.
TEST_F(ImmersiveModeControllerAshTest, TabAndBrowserFullscreen) {
  AddTab(browser(), GURL("about:blank"));

  // The shelf should start out as visible.
  ash::ShelfLayoutManager* shelf =
      ash::Shell::GetPrimaryRootWindowController()->GetShelfLayoutManager();
  ASSERT_EQ(ash::SHELF_VISIBLE, shelf->visibility_state());

  // 1) Test that entering tab fullscreen from immersive fullscreen hides the
  // tab indicators and the shelf.
  ToggleFullscreen();
  ASSERT_TRUE(controller()->IsEnabled());
  EXPECT_EQ(ash::SHELF_AUTO_HIDE, shelf->visibility_state());
  EXPECT_FALSE(controller()->ShouldHideTabIndicators());

  SetTabFullscreen(true);
  ASSERT_TRUE(controller()->IsEnabled());
  EXPECT_EQ(ash::SHELF_HIDDEN, shelf->visibility_state());
  EXPECT_TRUE(controller()->ShouldHideTabIndicators());

  // 2) Test that exiting tab fullscreen shows the tab indicators and autohides
  // the shelf.
  SetTabFullscreen(false);
  ASSERT_TRUE(controller()->IsEnabled());
  EXPECT_EQ(ash::SHELF_AUTO_HIDE, shelf->visibility_state());
  EXPECT_FALSE(controller()->ShouldHideTabIndicators());

  // 3) Test that exiting tab fullscreen and immersive fullscreen
  // simultaneously correctly updates the shelf visibility and whether the tab
  // indicators should be hidden.
  SetTabFullscreen(true);
  ToggleFullscreen();
  ASSERT_FALSE(controller()->IsEnabled());
  EXPECT_EQ(ash::SHELF_VISIBLE, shelf->visibility_state());
  EXPECT_TRUE(controller()->ShouldHideTabIndicators());
}

class ImmersiveModeControllerAshTestHostedApp
    : public ImmersiveModeControllerAshTest {
 public:
  ImmersiveModeControllerAshTestHostedApp()
      : ImmersiveModeControllerAshTest(Browser::TYPE_POPUP,
                                       chrome::HOST_DESKTOP_TYPE_ASH,
                                       true) {
  }
  virtual ~ImmersiveModeControllerAshTestHostedApp() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ImmersiveModeControllerAshTestHostedApp);
};

// Test the layout and visibility of the TopContainerView and web contents when
// a hosted app is put into immersive fullscreen.
TEST_F(ImmersiveModeControllerAshTestHostedApp, Layout) {
  // Add a tab because the browser starts out without any tabs at all.
  AddTab(browser(), GURL("about:blank"));

  TabStrip* tabstrip = browser_view()->tabstrip();
  ToolbarView* toolbar = browser_view()->toolbar();
  views::WebView* contents_web_view =
      browser_view()->GetContentsWebViewForTest();
  views::View* top_container = browser_view()->top_container();

  // Immersive fullscreen starts out disabled.
  ASSERT_FALSE(browser_view()->GetWidget()->IsFullscreen());
  ASSERT_FALSE(controller()->IsEnabled());

  // The tabstrip and toolbar are not visible for hosted apps.
  EXPECT_FALSE(tabstrip->visible());
  EXPECT_FALSE(toolbar->visible());

  // The window header should be above the web contents.
  int header_height = GetBoundsInWidget(contents_web_view).y();

  ToggleFullscreen();
  EXPECT_TRUE(browser_view()->GetWidget()->IsFullscreen());
  EXPECT_TRUE(controller()->IsEnabled());
  EXPECT_FALSE(controller()->IsRevealed());

  // Entering immersive fullscreen should make the web contents flush with the
  // top of the widget.
  EXPECT_FALSE(tabstrip->visible());
  EXPECT_FALSE(toolbar->visible());
  EXPECT_TRUE(top_container->GetVisibleBounds().IsEmpty());
  EXPECT_EQ(0, GetBoundsInWidget(contents_web_view).y());

  // Reveal the window header.
  AttemptReveal();

  // The tabstrip and toolbar should still be hidden and the web contents should
  // still be flush with the top of the screen.
  EXPECT_FALSE(tabstrip->visible());
  EXPECT_FALSE(toolbar->visible());
  EXPECT_EQ(0, GetBoundsInWidget(contents_web_view).y());

  // During an immersive reveal, the window header should be painted to the
  // TopContainerView. The TopContainerView should be flush with the top of the
  // widget and have |header_height|.
  gfx::Rect top_container_bounds_in_widget(GetBoundsInWidget(top_container));
  EXPECT_EQ(0, top_container_bounds_in_widget.y());
  EXPECT_EQ(header_height, top_container_bounds_in_widget.height());

  // Exit immersive fullscreen. The web contents should be back below the window
  // header.
  ToggleFullscreen();
  EXPECT_FALSE(browser_view()->GetWidget()->IsFullscreen());
  EXPECT_FALSE(controller()->IsEnabled());
  EXPECT_FALSE(tabstrip->visible());
  EXPECT_FALSE(toolbar->visible());
  EXPECT_EQ(header_height, GetBoundsInWidget(contents_web_view).y());
}
