// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/immersive_mode_controller.h"

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/rect.h"
#include "ui/views/view.h"

#if defined(USE_ASH)
#include "ash/root_window_controller.h"
#include "ash/shelf_types.h"
#include "ash/shell.h"
#include "ash/wm/shelf_layout_manager.h"
#endif

namespace {

// Returns the bounds of |view| in widget coordinates.
gfx::Rect GetRectInWidget(views::View* view) {
  return view->ConvertRectToWidget(view->GetLocalBounds());
}

}  // namespace

typedef InProcessBrowserTest ImmersiveModeControllerTest;

// TODO(linux_aura) http://crbug.com/163931
#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(USE_AURA)
#define MAYBE_ImmersiveMode DISABLED_ImmersiveMode
#else
#define MAYBE_ImmersiveMode ImmersiveMode
#endif

IN_PROC_BROWSER_TEST_F(ImmersiveModeControllerTest, MAYBE_ImmersiveMode) {
  ui::LayerAnimator::set_disable_animations_for_test(true);

  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
  ImmersiveModeController* controller =
      browser_view->immersive_mode_controller();
  views::View* contents_view = browser_view->GetTabContentsContainerView();
  browser_view->GetWidget()->Maximize();

  // Immersive mode is not on by default.
  EXPECT_FALSE(controller->enabled());
  EXPECT_FALSE(controller->ShouldHideTopViews());

  // Top-of-window views are visible.
  EXPECT_TRUE(browser_view->IsTabStripVisible());
  EXPECT_TRUE(browser_view->IsToolbarVisible());

  // Turning immersive mode on sets the toolbar to immersive style and hides
  // the top-of-window views while leaving the tab strip visible.
  controller->SetEnabled(true);
  EXPECT_TRUE(controller->enabled());
  EXPECT_TRUE(controller->ShouldHideTopViews());
  EXPECT_FALSE(controller->IsRevealed());
  EXPECT_TRUE(browser_view->tabstrip()->IsImmersiveStyle());
  EXPECT_TRUE(browser_view->IsTabStripVisible());
  EXPECT_FALSE(browser_view->IsToolbarVisible());
  // Content area is immediately below the tab indicators.
  EXPECT_EQ(GetRectInWidget(browser_view).y() + Tab::GetImmersiveHeight(),
            GetRectInWidget(contents_view).y());

  // Trigger a reveal keeps us in immersive mode, but top-of-window views
  // become visible.
  controller->StartRevealForTest();
  EXPECT_TRUE(controller->enabled());
  EXPECT_FALSE(controller->ShouldHideTopViews());
  EXPECT_TRUE(controller->IsRevealed());
  EXPECT_FALSE(browser_view->tabstrip()->IsImmersiveStyle());
  EXPECT_TRUE(browser_view->IsTabStripVisible());
  EXPECT_TRUE(browser_view->IsToolbarVisible());
  // Shelf hide triggered by enabling immersive mode eventually changes the
  // widget bounds and causes a Layout(). Force it to happen early for test.
  browser_view->parent()->Layout();
  // Content area is still immediately below the tab indicators.
  EXPECT_EQ(GetRectInWidget(browser_view).y() + Tab::GetImmersiveHeight(),
            GetRectInWidget(contents_view).y());

  // Ending a reveal keeps us in immersive mode, but toolbar goes invisible.
  controller->CancelReveal();
  EXPECT_TRUE(controller->enabled());
  EXPECT_TRUE(controller->ShouldHideTopViews());
  EXPECT_FALSE(controller->IsRevealed());
  EXPECT_TRUE(browser_view->tabstrip()->IsImmersiveStyle());
  EXPECT_TRUE(browser_view->IsTabStripVisible());
  EXPECT_FALSE(browser_view->IsToolbarVisible());

  // Disabling immersive mode puts us back to the beginning.
  controller->SetEnabled(false);
  EXPECT_FALSE(controller->enabled());
  EXPECT_FALSE(controller->ShouldHideTopViews());
  EXPECT_FALSE(controller->IsRevealed());
  EXPECT_FALSE(browser_view->tabstrip()->IsImmersiveStyle());
  EXPECT_TRUE(browser_view->IsTabStripVisible());
  EXPECT_TRUE(browser_view->IsToolbarVisible());

  // Disabling immersive mode while we are revealed should take us back to
  // the beginning.
  controller->SetEnabled(true);
  controller->StartRevealForTest();
  controller->SetEnabled(false);
  EXPECT_FALSE(controller->enabled());
  EXPECT_FALSE(controller->ShouldHideTopViews());
  EXPECT_FALSE(controller->IsRevealed());
  EXPECT_FALSE(browser_view->tabstrip()->IsImmersiveStyle());
  EXPECT_TRUE(browser_view->IsTabStripVisible());
  EXPECT_TRUE(browser_view->IsToolbarVisible());

  // When hiding the tab indicators, content is at the top of the browser view
  // both before and during reveal.
  controller->SetEnabled(false);
  controller->SetHideTabIndicatorsForTest(true);
  controller->SetEnabled(true);
  EXPECT_FALSE(browser_view->IsTabStripVisible());
  EXPECT_EQ(GetRectInWidget(browser_view).y(),
            GetRectInWidget(contents_view).y());
  controller->StartRevealForTest();
  EXPECT_TRUE(browser_view->IsTabStripVisible());
  // Shelf hide triggered by enabling immersive mode eventually changes the
  // widget bounds and causes a Layout(). Force it to happen early for test.
  browser_view->parent()->Layout();
  EXPECT_EQ(GetRectInWidget(browser_view).y(),
            GetRectInWidget(contents_view).y());
  controller->SetEnabled(false);
  controller->SetHideTabIndicatorsForTest(false);

  // Reveal ends when the mouse moves out of the reveal view.
  controller->SetEnabled(true);
  controller->StartRevealForTest();
  controller->OnRevealViewLostMouseForTest();
  EXPECT_FALSE(controller->IsRevealed());

  // Focus testing is not reliable on Windows, crbug.com/79493
#if !defined(OS_WIN)
  // Giving focus to the location bar prevents the reveal from ending when
  // the mouse exits.
  controller->SetEnabled(true);
  controller->StartRevealForTest();
  browser_view->SetFocusToLocationBar(false);
  controller->OnRevealViewLostMouseForTest();
  EXPECT_TRUE(controller->IsRevealed());

  // Releasing focus ends the reveal.
  browser_view->GetFocusManager()->ClearFocus();
  EXPECT_FALSE(controller->IsRevealed());

  // Placing focus in the location bar automatically causes a reveal.
  controller->SetEnabled(true);
  browser_view->SetFocusToLocationBar(false);
  EXPECT_TRUE(controller->IsRevealed());

  // Releasing focus ends the reveal again.
  browser_view->GetFocusManager()->ClearFocus();
  EXPECT_FALSE(controller->IsRevealed());
#endif  // defined(OS_WIN)

  // Window restore tracking is only implemented in the Aura port.
  // Also, Windows Aura does not trigger maximize/restore notifications.
#if defined(USE_AURA) && !defined(OS_WIN)
  // Restoring the window exits immersive mode.
  controller->SetEnabled(true);
  browser_view->GetWidget()->Restore();
  EXPECT_FALSE(controller->enabled());
  EXPECT_FALSE(controller->ShouldHideTopViews());
  EXPECT_FALSE(controller->IsRevealed());
  EXPECT_FALSE(browser_view->tabstrip()->IsImmersiveStyle());
  EXPECT_TRUE(browser_view->IsTabStripVisible());
  EXPECT_TRUE(browser_view->IsToolbarVisible());
#endif  // defined(USE_AURA) && !defined(OS_WIN)

  // Don't crash if we exit the test during a reveal.
  browser_view->GetWidget()->Maximize();
  controller->SetEnabled(true);
  controller->StartRevealForTest();
}

#if defined(USE_ASH)
// Test shelf auto-hide toggling behavior.
IN_PROC_BROWSER_TEST_F(ImmersiveModeControllerTest, ImmersiveShelf) {
  ui::LayerAnimator::set_disable_animations_for_test(true);

  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
  ImmersiveModeController* immersive_controller =
      browser_view->immersive_mode_controller();
  browser_view->GetWidget()->Maximize();

  // Shelf is visible when the test starts.
  ash::internal::ShelfLayoutManager* shelf =
      ash::Shell::GetPrimaryRootWindowController()->shelf();
  ASSERT_EQ(ash::SHELF_VISIBLE, shelf->visibility_state());

  // Turning immersive mode on sets the shelf to auto-hide.
  immersive_controller->SetEnabled(true);
  EXPECT_EQ(ash::SHELF_AUTO_HIDE, shelf->visibility_state());

  // Disabling immersive mode puts it back.
  immersive_controller->SetEnabled(false);
  EXPECT_EQ(ash::SHELF_VISIBLE, shelf->visibility_state());

  // The user could toggle the launcher behavior.
  shelf->SetAutoHideBehavior(ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  EXPECT_EQ(ash::SHELF_AUTO_HIDE, shelf->visibility_state());

  // Enabling immersive mode keeps auto-hide.
  immersive_controller->SetEnabled(true);
  EXPECT_EQ(ash::SHELF_AUTO_HIDE, shelf->visibility_state());

  // Disabling immersive mode maintains the user's auto-hide selection.
  immersive_controller->SetEnabled(false);
  EXPECT_EQ(ash::SHELF_AUTO_HIDE, shelf->visibility_state());
}
#endif  // defined(OS_CHROMEOS)
