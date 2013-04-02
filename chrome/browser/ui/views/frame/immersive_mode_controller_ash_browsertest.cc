// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/immersive_mode_controller_ash.h"

#include "ash/ash_switches.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_types.h"
#include "ash/shell.h"
#include "ash/wm/window_properties.h"
#include "base/command_line.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/ui/app_modal_dialogs/app_modal_dialog_queue.h"
#include "chrome/browser/ui/app_modal_dialogs/javascript_dialog_manager.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/browser_dialogs.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/rect.h"
#include "ui/views/view.h"

namespace {

// Returns the bounds of |view| in widget coordinates.
gfx::Rect GetRectInWidget(views::View* view) {
  return view->ConvertRectToWidget(view->GetLocalBounds());
}

}  // namespace

// TODO(jamescook): If immersive mode becomes popular on CrOS, consider porting
// it to other Aura platforms (win_aura, linux_aura).  http://crbug.com/163931
#if defined(OS_CHROMEOS)

class ImmersiveModeControllerAshTest : public InProcessBrowserTest {
 public:
  ImmersiveModeControllerAshTest() {}
  virtual ~ImmersiveModeControllerAshTest() {}

  // Callback for when the onbeforeunload dialog closes for the sake of testing
  // the dialog with immersive mode.
  void OnBeforeUnloadJavaScriptDialogClosed(
      bool success,
      const string16& user_input) {
  }

  // content::BrowserTestBase overrides:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(ash::switches::kAshImmersiveFullscreen);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ImmersiveModeControllerAshTest);
};

IN_PROC_BROWSER_TEST_F(ImmersiveModeControllerAshTest, ImmersiveMode) {
  ui::ScopedAnimationDurationScaleMode zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  ASSERT_TRUE(chrome::UseImmersiveFullscreen());

  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
  ImmersiveModeControllerAsh* controller =
      static_cast<ImmersiveModeControllerAsh*>(
          browser_view->immersive_mode_controller());
  views::View* contents_view = browser_view->GetTabContentsContainerView();

  // Immersive mode is not on by default.
  EXPECT_FALSE(controller->IsEnabled());
  EXPECT_FALSE(controller->ShouldHideTopViews());

  // Top-of-window views are visible.
  EXPECT_TRUE(browser_view->IsTabStripVisible());
  EXPECT_TRUE(browser_view->IsToolbarVisible());

  // Usual commands are enabled.
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_OPEN_CURRENT_URL));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_ABOUT));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FULLSCREEN));

  // Turning immersive mode on sets the toolbar to immersive style and hides
  // the top-of-window views while leaving the tab strip visible.
  chrome::ToggleFullscreenMode(browser());
  ASSERT_TRUE(browser_view->IsFullscreen());
  EXPECT_TRUE(controller->IsEnabled());
  EXPECT_TRUE(controller->ShouldHideTopViews());
  EXPECT_FALSE(controller->IsRevealed());
  EXPECT_TRUE(browser_view->tabstrip()->IsImmersiveStyle());
  EXPECT_TRUE(browser_view->IsTabStripVisible());
  EXPECT_FALSE(browser_view->IsToolbarVisible());
  // Content area is immediately below the tab indicators.
  EXPECT_EQ(GetRectInWidget(browser_view).y() + Tab::GetImmersiveHeight(),
            GetRectInWidget(contents_view).y());

  // Commands are still enabled (usually fullscreen disables these).
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_OPEN_CURRENT_URL));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_ABOUT));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FULLSCREEN));

  // Trigger a reveal keeps us in immersive mode, but top-of-window views
  // become visible.
  controller->StartRevealForTest(true);
  EXPECT_TRUE(controller->IsEnabled());
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
  EXPECT_TRUE(controller->IsEnabled());
  EXPECT_TRUE(controller->ShouldHideTopViews());
  EXPECT_FALSE(controller->IsRevealed());
  EXPECT_TRUE(browser_view->tabstrip()->IsImmersiveStyle());
  EXPECT_TRUE(browser_view->IsTabStripVisible());
  EXPECT_FALSE(browser_view->IsToolbarVisible());

  // Disabling immersive mode puts us back to the beginning.
  chrome::ToggleFullscreenMode(browser());
  ASSERT_FALSE(browser_view->IsFullscreen());
  EXPECT_FALSE(controller->IsEnabled());
  EXPECT_FALSE(controller->ShouldHideTopViews());
  EXPECT_FALSE(controller->IsRevealed());
  EXPECT_FALSE(browser_view->tabstrip()->IsImmersiveStyle());
  EXPECT_TRUE(browser_view->IsTabStripVisible());
  EXPECT_TRUE(browser_view->IsToolbarVisible());

  // Disabling immersive mode while we are revealed should take us back to
  // the beginning.
  chrome::ToggleFullscreenMode(browser());
  ASSERT_TRUE(browser_view->IsFullscreen());
  EXPECT_TRUE(controller->IsEnabled());
  controller->StartRevealForTest(true);

  chrome::ToggleFullscreenMode(browser());
  ASSERT_FALSE(browser_view->IsFullscreen());
  EXPECT_FALSE(controller->IsEnabled());
  EXPECT_FALSE(controller->ShouldHideTopViews());
  EXPECT_FALSE(controller->IsRevealed());
  EXPECT_FALSE(browser_view->tabstrip()->IsImmersiveStyle());
  EXPECT_TRUE(browser_view->IsTabStripVisible());
  EXPECT_TRUE(browser_view->IsToolbarVisible());

  // When hiding the tab indicators, content is at the top of the browser view
  // both before and during reveal.
  controller->SetHideTabIndicatorsForTest(true);
  chrome::ToggleFullscreenMode(browser());
  ASSERT_TRUE(browser_view->IsFullscreen());
  EXPECT_FALSE(browser_view->IsTabStripVisible());
  EXPECT_EQ(GetRectInWidget(browser_view).y(),
            GetRectInWidget(contents_view).y());
  controller->StartRevealForTest(true);
  EXPECT_TRUE(browser_view->IsTabStripVisible());
  // Shelf hide triggered by enabling immersive mode eventually changes the
  // widget bounds and causes a Layout(). Force it to happen early for test.
  browser_view->parent()->Layout();
  EXPECT_EQ(GetRectInWidget(browser_view).y(),
            GetRectInWidget(contents_view).y());
  chrome::ToggleFullscreenMode(browser());
  ASSERT_FALSE(browser_view->IsFullscreen());
  controller->SetHideTabIndicatorsForTest(false);

  // Reveal ends when the mouse moves out of the reveal view.
  chrome::ToggleFullscreenMode(browser());
  ASSERT_TRUE(browser_view->IsFullscreen());
  EXPECT_TRUE(controller->IsEnabled());
  controller->StartRevealForTest(true);
  controller->SetMouseHoveredForTest(false);
  EXPECT_FALSE(controller->IsRevealed());

  // Window restore tracking is only implemented in the Aura port.
  // Also, Windows Aura does not trigger maximize/restore notifications.
#if !defined(OS_WIN)
  // Restoring the window exits immersive mode.
  browser_view->GetWidget()->Restore();
  ASSERT_FALSE(browser_view->IsFullscreen());
  EXPECT_FALSE(controller->IsEnabled());
  EXPECT_FALSE(controller->ShouldHideTopViews());
  EXPECT_FALSE(controller->IsRevealed());
  EXPECT_FALSE(browser_view->tabstrip()->IsImmersiveStyle());
  EXPECT_TRUE(browser_view->IsTabStripVisible());
  EXPECT_TRUE(browser_view->IsToolbarVisible());
#endif  // !defined(OS_WIN)

  // Don't crash if we exit the test during a reveal.
  if (!browser_view->IsFullscreen())
    chrome::ToggleFullscreenMode(browser());
  ASSERT_TRUE(browser_view->IsFullscreen());
  ASSERT_TRUE(controller->IsEnabled());
  controller->StartRevealForTest(true);
}

// Test how focus affects whether the top-of-window views are revealed.
// Do not test under windows because focus testing is not reliable on
// Windows, crbug.com/79493
#if !defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(ImmersiveModeControllerAshTest, Focus) {
  ui::ScopedAnimationDurationScaleMode zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  ASSERT_TRUE(chrome::UseImmersiveFullscreen());
  chrome::ToggleFullscreenMode(browser());

  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
  ImmersiveModeControllerAsh* controller =
      static_cast<ImmersiveModeControllerAsh*>(
          browser_view->immersive_mode_controller());

  // 1) Test that focusing the location bar automatically reveals the
  // top-of-window views.
  //
  // Move the mouse of the way.
  controller->SetMouseHoveredForTest(false);
  browser_view->SetFocusToLocationBar(false);
  EXPECT_TRUE(controller->IsRevealed());
  browser_view->GetFocusManager()->ClearFocus();
  EXPECT_FALSE(controller->IsRevealed());

  // 2) Test that the top-of-window views stay revealed as long as either the
  // location bar has focus or the mouse is hovered above the top-of-window
  // views.
  controller->StartRevealForTest(true);
  browser_view->SetFocusToLocationBar(false);
  browser_view->GetFocusManager()->ClearFocus();
  EXPECT_TRUE(controller->IsRevealed());
  browser_view->SetFocusToLocationBar(false);
  controller->SetMouseHoveredForTest(false);
  EXPECT_TRUE(controller->IsRevealed());
  browser_view->GetFocusManager()->ClearFocus();
  EXPECT_FALSE(controller->IsRevealed());

  // 3) Test that a bubble keeps the top-of-window views revealed for the
  // duration of its visibity.
  //
  // Setup so that the bookmark bubble actually shows.
  ui_test_utils::WaitForBookmarkModelToLoad(
      BookmarkModelFactory::GetForProfile(browser()->profile()));
  browser_view->Activate();
  EXPECT_TRUE(browser_view->IsActive());

  controller->StartRevealForTest(false);
  chrome::ExecuteCommand(browser(), IDC_BOOKMARK_PAGE_FROM_STAR);
  EXPECT_TRUE(chrome::IsBookmarkBubbleViewShowing());
  EXPECT_TRUE(controller->IsRevealed());
  chrome::HideBookmarkBubbleView();
  EXPECT_FALSE(controller->IsRevealed());

  // 4) Test that focusing the web contents hides the top-of-window views.
  browser_view->SetFocusToLocationBar(false);
  EXPECT_TRUE(controller->IsRevealed());
  browser_view->GetTabContentsContainerView()->RequestFocus();
  EXPECT_FALSE(controller->IsRevealed());

  // 5) Test that a dialog opened by the web contents does not initiate a
  // reveal.
  AppModalDialogQueue* queue = AppModalDialogQueue::GetInstance();
  EXPECT_FALSE(queue->HasActiveDialog());
  content::WebContents* web_contents = browser_view->GetActiveWebContents();
  GetJavaScriptDialogManagerInstance()->RunBeforeUnloadDialog(
      web_contents,
      string16(),
      false,
      base::Bind(
          &ImmersiveModeControllerAshTest::OnBeforeUnloadJavaScriptDialogClosed,
          base::Unretained(this)));
  EXPECT_TRUE(queue->HasActiveDialog());
  EXPECT_FALSE(controller->IsRevealed());
}
#endif  // OS_WIN

// Test behavior when the mouse becomes hovered without moving.
IN_PROC_BROWSER_TEST_F(ImmersiveModeControllerAshTest,
                       MouseHoveredWithoutMoving) {
  ui::ScopedAnimationDurationScaleMode zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  ASSERT_TRUE(chrome::UseImmersiveFullscreen());
  chrome::ToggleFullscreenMode(browser());

  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
  ImmersiveModeControllerAsh* controller =
      static_cast<ImmersiveModeControllerAsh*>(
          browser_view->immersive_mode_controller());
  scoped_ptr<ImmersiveModeController::RevealedLock> lock(NULL);

  // 1) Test that if the mouse becomes hovered without the mouse moving due to a
  // lock causing the top-of-window views to be revealed (and the mouse
  // happening to be near the top of the screen), the top-of-window views do not
  // hide till the mouse moves off of the top-of-window views.
  controller->SetMouseHoveredForTest(true);
  EXPECT_FALSE(controller->IsRevealed());
  lock.reset(controller->GetRevealedLock());
  EXPECT_TRUE(controller->IsRevealed());
  lock.reset();
  EXPECT_TRUE(controller->IsRevealed());
  controller->SetMouseHoveredForTest(false);
  EXPECT_FALSE(controller->IsRevealed());

  // 2) Test that if the mouse becomes hovered without moving because of a
  // reveal in ImmersiveModeController::SetEnabled(true) and there are no locks
  // keeping the top-of-window views revealed, that mouse hover does not prevent
  // the top-of-window views from closing.
  chrome::ToggleFullscreenMode(browser());
  controller->SetMouseHoveredForTest(true);
  EXPECT_FALSE(controller->IsRevealed());
  chrome::ToggleFullscreenMode(browser());
  EXPECT_FALSE(controller->IsRevealed());

  // 3) Test that if the mouse becomes hovered without moving because of a
  // reveal in ImmersiveModeController::SetEnabled(true) and there is a lock
  // keeping the top-of-window views revealed, that the top-of-window views do
  // not hide till the mouse moves off of the top-of-window views.
  chrome::ToggleFullscreenMode(browser());
  controller->SetMouseHoveredForTest(true);
  lock.reset(controller->GetRevealedLock());
  EXPECT_FALSE(controller->IsRevealed());
  chrome::ToggleFullscreenMode(browser());
  EXPECT_TRUE(controller->IsRevealed());
  lock.reset();
  EXPECT_TRUE(controller->IsRevealed());
  controller->SetMouseHoveredForTest(false);
  EXPECT_FALSE(controller->IsRevealed());
}

// GetRevealedLock() specific tests.
IN_PROC_BROWSER_TEST_F(ImmersiveModeControllerAshTest, RevealedLock) {
  scoped_ptr<ImmersiveModeController::RevealedLock> lock1(NULL);
  scoped_ptr<ImmersiveModeController::RevealedLock> lock2(NULL);

  ui::ScopedAnimationDurationScaleMode zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  ASSERT_TRUE(chrome::UseImmersiveFullscreen());

  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
  ImmersiveModeControllerAsh* controller =
      static_cast<ImmersiveModeControllerAsh*>(
          browser_view->immersive_mode_controller());

  // Immersive mode is not on by default.
  EXPECT_FALSE(controller->IsEnabled());

  // Move the mouse out of the way.
  controller->SetMouseHoveredForTest(false);

  // 1) Test acquiring and releasing a revealed state lock while immersive mode
  // is disabled. Acquiring or releasing the lock should have no effect till
  // immersive mode is enabled.
  lock1.reset(controller->GetRevealedLock());
  EXPECT_FALSE(controller->IsEnabled());
  EXPECT_FALSE(controller->IsRevealed());
  EXPECT_FALSE(controller->ShouldHideTopViews());

  // Immersive mode should start in the revealed state due to the lock.
  chrome::ToggleFullscreenMode(browser());
  EXPECT_TRUE(controller->IsEnabled());
  EXPECT_TRUE(controller->IsRevealed());
  EXPECT_FALSE(controller->ShouldHideTopViews());

  chrome::ToggleFullscreenMode(browser());
  EXPECT_FALSE(controller->IsEnabled());
  EXPECT_FALSE(controller->IsRevealed());
  EXPECT_FALSE(controller->ShouldHideTopViews());

  lock1.reset();
  EXPECT_FALSE(controller->IsEnabled());
  EXPECT_FALSE(controller->IsRevealed());
  EXPECT_FALSE(controller->ShouldHideTopViews());

  // Immersive mode should start in the closed state because the lock is no
  // longer held.
  chrome::ToggleFullscreenMode(browser());
  EXPECT_TRUE(controller->IsEnabled());
  EXPECT_FALSE(controller->IsRevealed());
  EXPECT_TRUE(controller->ShouldHideTopViews());

  // 2) Test that acquiring a revealed state lock reveals the top-of-window
  // views if they are hidden.
  controller->CancelReveal();
  EXPECT_FALSE(controller->IsRevealed());
  lock1.reset(controller->GetRevealedLock());
  EXPECT_TRUE(controller->IsRevealed());

  // 3) Test that the top-of-window views are only hidden when all of the locks
  // are released.
  lock2.reset(controller->GetRevealedLock());
  lock1.reset();
  EXPECT_TRUE(controller->IsRevealed());

  lock2.reset();
  EXPECT_FALSE(controller->IsRevealed());
}

// Shelf-specific immersive mode tests.
IN_PROC_BROWSER_TEST_F(ImmersiveModeControllerAshTest, ImmersiveShelf) {
  ui::ScopedAnimationDurationScaleMode zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  ASSERT_TRUE(chrome::UseImmersiveFullscreen());

  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
  ImmersiveModeControllerAsh* immersive_controller =
      static_cast<ImmersiveModeControllerAsh*>(
          browser_view->immersive_mode_controller());

  // Shelf is visible when the test starts.
  ash::internal::ShelfLayoutManager* shelf =
      ash::Shell::GetPrimaryRootWindowController()->GetShelfLayoutManager();
  ASSERT_EQ(ash::SHELF_VISIBLE, shelf->visibility_state());

  // Turning immersive mode on sets the shelf to auto-hide.
  chrome::ToggleFullscreenMode(browser());
  ASSERT_TRUE(browser_view->IsFullscreen());
  ASSERT_TRUE(immersive_controller->IsEnabled());
  EXPECT_EQ(ash::SHELF_AUTO_HIDE, shelf->visibility_state());

  // Disabling immersive mode puts it back.
  chrome::ToggleFullscreenMode(browser());
  ASSERT_FALSE(browser_view->IsFullscreen());
  ASSERT_FALSE(immersive_controller->IsEnabled());
  EXPECT_EQ(ash::SHELF_VISIBLE, shelf->visibility_state());

  // The user could toggle the launcher behavior.
  shelf->SetAutoHideBehavior(ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  EXPECT_EQ(ash::SHELF_AUTO_HIDE, shelf->visibility_state());

  // Enabling immersive mode keeps auto-hide.
  chrome::ToggleFullscreenMode(browser());
  ASSERT_TRUE(browser_view->IsFullscreen());
  ASSERT_TRUE(immersive_controller->IsEnabled());
  EXPECT_EQ(ash::SHELF_AUTO_HIDE, shelf->visibility_state());

  // Disabling immersive mode maintains the user's auto-hide selection.
  chrome::ToggleFullscreenMode(browser());
  ASSERT_FALSE(browser_view->IsFullscreen());
  ASSERT_FALSE(immersive_controller->IsEnabled());
  EXPECT_EQ(ash::SHELF_AUTO_HIDE, shelf->visibility_state());

  // Setting the window property directly toggles immersive mode.
  aura::Window* window = browser_view->GetWidget()->GetNativeWindow();
  window->SetProperty(ash::internal::kImmersiveModeKey, true);
  EXPECT_TRUE(immersive_controller->IsEnabled());
  window->SetProperty(ash::internal::kImmersiveModeKey, false);
  EXPECT_FALSE(immersive_controller->IsEnabled());
}

#endif  // defined(OS_CHROMEOS)
