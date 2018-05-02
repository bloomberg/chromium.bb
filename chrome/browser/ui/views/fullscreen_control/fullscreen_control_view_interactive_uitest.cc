// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_bubble_type.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/fullscreen_control/fullscreen_control_host.h"
#include "chrome/browser/ui/views/fullscreen_control/fullscreen_control_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_fullscreen_options.h"
#include "ui/events/event.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"
#include "url/gurl.h"

namespace {

constexpr base::TimeDelta kPopupEventTimeout = base::TimeDelta::FromSeconds(5);

// Observer for NOTIFICATION_FULLSCREEN_CHANGED notifications.
class FullscreenNotificationObserver
    : public content::WindowedNotificationObserver {
 public:
  FullscreenNotificationObserver()
      : WindowedNotificationObserver(
            chrome::NOTIFICATION_FULLSCREEN_CHANGED,
            content::NotificationService::AllSources()) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FullscreenNotificationObserver);
};

}  // namespace

class FullscreenControlViewTest : public InProcessBrowserTest {
 public:
  FullscreenControlViewTest() = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kFullscreenExitUI, features::kKeyboardLockAPI}, {});
    InProcessBrowserTest::SetUp();
  }

 protected:
  FullscreenControlHost* GetFullscreenControlHost() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    return browser_view->GetFullscreenControlHost();
  }

  FullscreenControlView* GetFullscreenControlView() {
    return GetFullscreenControlHost()->fullscreen_control_view();
  }

  views::Button* GetFullscreenExitButton() {
    return GetFullscreenControlView()->exit_fullscreen_button();
  }

  ExclusiveAccessManager* GetExclusiveAccessManager() {
    return browser()->exclusive_access_manager();
  }

  KeyboardLockController* GetKeyboardLockController() {
    return GetExclusiveAccessManager()->keyboard_lock_controller();
  }

  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void EnterActiveTabFullscreen() {
    FullscreenNotificationObserver fullscreen_observer;
    content::WebContentsDelegate* delegate =
        static_cast<content::WebContentsDelegate*>(browser());
    delegate->EnterFullscreenModeForTab(GetActiveWebContents(),
                                        GURL("about:blank"),
                                        blink::WebFullscreenOptions());
    fullscreen_observer.Wait();
    ASSERT_TRUE(delegate->IsFullscreenForTabOrPending(GetActiveWebContents()));
  }

  void EnableKeyboardLock() {
    KeyboardLockController* controller = GetKeyboardLockController();
    controller->fake_keyboard_lock_for_test_ = true;
    controller->RequestKeyboardLock(GetActiveWebContents(),
                                    /*require_esc_to_exit=*/true);
    controller->fake_keyboard_lock_for_test_ = false;
  }

  void SetPopupVisibilityChangedCallback(base::OnceClosure callback) {
    GetFullscreenControlHost()->on_popup_visibility_changed_ =
        std::move(callback);
  }

  base::OneShotTimer* GetPopupTimeoutTimer() {
    return &GetFullscreenControlHost()->popup_timeout_timer_;
  }

  void RunLoopUntilVisibilityChanges() {
    base::RunLoop run_loop;
    SetPopupVisibilityChangedCallback(run_loop.QuitClosure());
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), kPopupEventTimeout);
    run_loop.Run();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(FullscreenControlViewTest);
};

#if defined(OS_MACOSX)
// Entering fullscreen is flaky on Mac: http://crbug.com/824517
#define MAYBE_MouseExitFullscreen DISABLED_MouseExitFullscreen
#else
#define MAYBE_MouseExitFullscreen MouseExitFullscreen
#endif
IN_PROC_BROWSER_TEST_F(FullscreenControlViewTest, MAYBE_MouseExitFullscreen) {
  EnterActiveTabFullscreen();
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  ASSERT_TRUE(browser_view->IsFullscreen());

  FullscreenControlHost* host = GetFullscreenControlHost();
  host->Hide(false);
  ASSERT_FALSE(host->IsVisible());

  // Simulate moving the mouse to the top of the screen, which should show the
  // fullscreen exit UI.
  ui::MouseEvent mouse_move(ui::ET_MOUSE_MOVED, gfx::Point(1, 1), gfx::Point(),
                            base::TimeTicks(), 0, 0);
  host->OnMouseEvent(&mouse_move);
  ASSERT_TRUE(host->IsVisible());

  // Simulate clicking on the fullscreen exit button, which should cause the
  // browser to exit fullscreen and hide the fullscreen exit control.
  ui::MouseEvent mouse_click(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                             base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  GetFullscreenControlView()->ButtonPressed(GetFullscreenExitButton(),
                                            mouse_click);

  ASSERT_FALSE(host->IsVisible());
  ASSERT_FALSE(browser_view->IsFullscreen());
}

#if defined(OS_MACOSX)
// Entering fullscreen is flaky on Mac: http://crbug.com/824517
#define MAYBE_MouseExitFullscreen_TimeoutAndRetrigger \
  DISABLED_MouseExitFullscreen_TimeoutAndRetrigger
#else
#define MAYBE_MouseExitFullscreen_TimeoutAndRetrigger \
  MouseExitFullscreen_TimeoutAndRetrigger
#endif
IN_PROC_BROWSER_TEST_F(FullscreenControlViewTest,
                       MAYBE_MouseExitFullscreen_TimeoutAndRetrigger) {
  EnterActiveTabFullscreen();
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  ASSERT_TRUE(browser_view->IsFullscreen());

  FullscreenControlHost* host = GetFullscreenControlHost();
  host->Hide(false);
  ASSERT_FALSE(host->IsVisible());

  // Simulate moving the mouse to the top of the screen, which should show the
  // fullscreen exit UI.
  ui::MouseEvent mouse_move(ui::ET_MOUSE_MOVED, gfx::Point(1, 1), gfx::Point(),
                            base::TimeTicks(), 0, 0);
  host->OnMouseEvent(&mouse_move);
  ASSERT_TRUE(host->IsVisible());

  // Wait until popup times out. This is one wait for show and one wait for
  // hide.
  RunLoopUntilVisibilityChanges();
  RunLoopUntilVisibilityChanges();
  ASSERT_FALSE(host->IsVisible());

  // Simulate moving the mouse to the top again. This should not show the exit
  // UI.
  mouse_move = ui::MouseEvent(ui::ET_MOUSE_MOVED, gfx::Point(2, 1),
                              gfx::Point(), base::TimeTicks(), 0, 0);
  host->OnMouseEvent(&mouse_move);
  ASSERT_FALSE(host->IsVisible());

  // Simulate moving the mouse out of the buffer area. This resets the state.
  mouse_move = ui::MouseEvent(ui::ET_MOUSE_MOVED, gfx::Point(2, 1000),
                              gfx::Point(), base::TimeTicks(), 0, 0);
  host->OnMouseEvent(&mouse_move);
  ASSERT_FALSE(host->IsVisible());

  // Simulate moving the mouse to the top again, which should show the exit UI.
  mouse_move = ui::MouseEvent(ui::ET_MOUSE_MOVED, gfx::Point(1, 1),
                              gfx::Point(), base::TimeTicks(), 0, 0);
  host->OnMouseEvent(&mouse_move);
  RunLoopUntilVisibilityChanges();
  ASSERT_TRUE(host->IsVisible());

  // Simulate immediately moving the mouse out of the buffer area. This should
  // hide the exit UI.
  mouse_move = ui::MouseEvent(ui::ET_MOUSE_MOVED, gfx::Point(2, 1000),
                              gfx::Point(), base::TimeTicks(), 0, 0);
  host->OnMouseEvent(&mouse_move);
  RunLoopUntilVisibilityChanges();
  ASSERT_FALSE(host->IsVisible());

  ASSERT_TRUE(browser_view->IsFullscreen());
}

#if defined(OS_MACOSX)
// Entering fullscreen is flaky on Mac: http://crbug.com/824517
#define MAYBE_KeyboardPopupInteraction DISABLED_KeyboardPopupInteraction
#else
#define MAYBE_KeyboardPopupInteraction KeyboardPopupInteraction
#endif
IN_PROC_BROWSER_TEST_F(FullscreenControlViewTest,
                       MAYBE_KeyboardPopupInteraction) {
  // TODO(yuweih): Test will fail if the cursor is at the top of the screen.
  // That's because random mouse move events may pass from the browser to the
  // host and interfere with the InputEntryMethod tracking. Consider fixing
  // this.
  EnterActiveTabFullscreen();
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  ASSERT_TRUE(browser_view->IsFullscreen());

  FullscreenControlHost* host = GetFullscreenControlHost();
  host->Hide(false);
  ASSERT_FALSE(host->IsVisible());

  // Lock the keyboard and ensure it is active.
  EnableKeyboardLock();
  ASSERT_TRUE(GetKeyboardLockController()->IsKeyboardLockActive());

  // Verify a bubble message is now displayed, then dismiss it.
  ASSERT_NE(ExclusiveAccessBubbleType::EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE,
            GetExclusiveAccessManager()->GetExclusiveAccessExitBubbleType());
  host->Hide(/*animate=*/false);
  ASSERT_FALSE(host->IsVisible());

  base::RunLoop show_run_loop;
  SetPopupVisibilityChangedCallback(show_run_loop.QuitClosure());
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, show_run_loop.QuitClosure(), kPopupEventTimeout);

  // Send a key press event to show the popup.
  ui::KeyEvent key_down(ui::ET_KEY_PRESSED, ui::VKEY_ESCAPE, ui::EF_NONE);
  host->OnKeyEvent(&key_down);
  // Popup is not shown immediately.
  ASSERT_FALSE(host->IsVisible());

  show_run_loop.Run();
  ASSERT_TRUE(host->IsVisible());

  base::RunLoop hide_run_loop;
  SetPopupVisibilityChangedCallback(hide_run_loop.QuitClosure());
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, hide_run_loop.QuitClosure(), kPopupEventTimeout);

  // Send a key press event to hide the popup.
  ui::KeyEvent key_up(ui::ET_KEY_RELEASED, ui::VKEY_ESCAPE, ui::EF_NONE);
  host->OnKeyEvent(&key_up);
  hide_run_loop.Run();
  ASSERT_FALSE(host->IsVisible());
}
