// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/immersive_mode_controller.h"

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "ui/compositor/layer_animator.h"

typedef InProcessBrowserTest ImmersiveModeControllerTest;

IN_PROC_BROWSER_TEST_F(ImmersiveModeControllerTest, ImmersiveMode) {
  ui::LayerAnimator::set_disable_animations_for_test(true);

  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
  ImmersiveModeController* controller =
      browser_view->immersive_mode_controller();
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

  // Trigger a reveal keeps us in immersive mode, but top-of-window views
  // become visible.
  controller->StartRevealForTest();
  EXPECT_TRUE(controller->enabled());
  EXPECT_FALSE(controller->ShouldHideTopViews());
  EXPECT_TRUE(controller->IsRevealed());
  EXPECT_FALSE(browser_view->tabstrip()->IsImmersiveStyle());
  EXPECT_TRUE(browser_view->IsTabStripVisible());
  EXPECT_TRUE(browser_view->IsToolbarVisible());

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
