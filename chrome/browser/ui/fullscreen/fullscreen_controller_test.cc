// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/fullscreen/fullscreen_controller_test.h"

#include "base/command_line.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/fullscreen/fullscreen_controller.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_navigation_observer.h"

using content::WebContents;

const char FullscreenControllerTest::kFullscreenMouseLockHTML[] =
    "files/fullscreen_mouselock/fullscreen_mouselock.html";

void FullscreenControllerTest::SetUpCommandLine(CommandLine* command_line) {
  InProcessBrowserTest::SetUpCommandLine(command_line);
  command_line->AppendSwitch(switches::kEnablePointerLock);
}

void FullscreenControllerTest::ToggleTabFullscreen(bool enter_fullscreen) {
  ToggleTabFullscreen_Internal(enter_fullscreen, true);
}

// |ToggleTabFullscreen| should not need to tolerate the transition failing.
// Most fullscreen tests run sharded in fullscreen_controller_browsertest.cc
// and some flakiness has occurred when calling |ToggleTabFullscreen|, so that
// method has been made robust by retrying if the transition fails.
// The root cause of that flakiness should still be tracked down, see
// http://crbug.com/133831. In the mean time, this method
// allows a fullscreen_controller_interactive_browsertest.cc test to verify
// that when running serially there is no flakiness in the transition.
void FullscreenControllerTest::ToggleTabFullscreenNoRetries(
    bool enter_fullscreen) {
  ToggleTabFullscreen_Internal(enter_fullscreen, false);
}

void FullscreenControllerTest::ToggleBrowserFullscreen(bool enter_fullscreen) {
  ASSERT_EQ(browser()->window()->IsFullscreen(), !enter_fullscreen);
  FullscreenNotificationObserver fullscreen_observer;

  browser()->ToggleFullscreenMode();

  fullscreen_observer.Wait();
  ASSERT_EQ(browser()->window()->IsFullscreen(), enter_fullscreen);
  ASSERT_EQ(IsFullscreenForBrowser(), enter_fullscreen);
}

void FullscreenControllerTest::RequestToLockMouse(
    bool user_gesture,
    bool last_unlocked_by_target) {
  WebContents* tab = browser()->GetActiveWebContents();
  browser()->RequestToLockMouse(tab, user_gesture,
      last_unlocked_by_target);
}

void FullscreenControllerTest::LostMouseLock() {
  browser()->LostMouseLock();
}

bool FullscreenControllerTest::SendEscapeToFullscreenController() {
  return browser()->fullscreen_controller_->HandleUserPressedEscape();
}

bool FullscreenControllerTest::IsFullscreenForBrowser() {
  return browser()->fullscreen_controller_->IsFullscreenForBrowser();
}

bool FullscreenControllerTest::IsFullscreenForTabOrPending() {
  return browser()->IsFullscreenForTabOrPending();
}

bool FullscreenControllerTest::IsMouseLockPermissionRequested() {
  FullscreenExitBubbleType type =
      browser()->fullscreen_controller_->GetFullscreenExitBubbleType();
  bool mouse_lock = false;
  fullscreen_bubble::PermissionRequestedByType(type, NULL, &mouse_lock);
  return mouse_lock;
}

bool FullscreenControllerTest::IsFullscreenPermissionRequested() {
  FullscreenExitBubbleType type =
      browser()->fullscreen_controller_->GetFullscreenExitBubbleType();
  bool fullscreen = false;
  fullscreen_bubble::PermissionRequestedByType(type, &fullscreen, NULL);
  return fullscreen;
}

FullscreenExitBubbleType
    FullscreenControllerTest::GetFullscreenExitBubbleType() {
  return browser()->fullscreen_controller_->GetFullscreenExitBubbleType();
}

bool FullscreenControllerTest::IsFullscreenBubbleDisplayed() {
  FullscreenExitBubbleType type =
      browser()->fullscreen_controller_->GetFullscreenExitBubbleType();
  return type != FEB_TYPE_NONE;
}

bool FullscreenControllerTest::IsFullscreenBubbleDisplayingButtons() {
  FullscreenExitBubbleType type =
      browser()->fullscreen_controller_->GetFullscreenExitBubbleType();
  return fullscreen_bubble::ShowButtonsForType(type);
}

void FullscreenControllerTest::AcceptCurrentFullscreenOrMouseLockRequest() {
  WebContents* fullscreen_tab = browser()->GetActiveWebContents();
  FullscreenExitBubbleType type =
      browser()->fullscreen_controller_->GetFullscreenExitBubbleType();
  browser()->OnAcceptFullscreenPermission(fullscreen_tab->GetURL(), type);
}

void FullscreenControllerTest::DenyCurrentFullscreenOrMouseLockRequest() {
  FullscreenExitBubbleType type =
      browser()->fullscreen_controller_->GetFullscreenExitBubbleType();
  browser()->OnDenyFullscreenPermission(type);
}

void FullscreenControllerTest::AddTabAtIndexAndWait(int index, const GURL& url,
    content::PageTransition transition) {
  content::TestNavigationObserver observer(
      content::NotificationService::AllSources(), NULL, 1);

  AddTabAtIndex(index, url, transition);

  observer.Wait();
}

void FullscreenControllerTest::GoBack() {
  content::TestNavigationObserver observer(
      content::NotificationService::AllSources(), NULL, 1);

  chrome::GoBack(browser(), CURRENT_TAB);

  observer.Wait();
}

void FullscreenControllerTest::Reload() {
  content::TestNavigationObserver observer(
      content::NotificationService::AllSources(), NULL, 1);

  chrome::Reload(browser(), CURRENT_TAB);

  observer.Wait();
}

void FullscreenControllerTest::ToggleTabFullscreen_Internal(
    bool enter_fullscreen, bool retry_until_success) {
  WebContents* tab = browser()->GetActiveWebContents();
  if (IsFullscreenForBrowser()) {
    // Changing tab fullscreen state will not actually change the window
    // when browser fullscreen is in effect.
    browser()->ToggleFullscreenModeForTab(tab, enter_fullscreen);
  } else {  // Not in browser fullscreen, expect window to actually change.
    ASSERT_NE(browser()->window()->IsFullscreen(), enter_fullscreen);
    do {
      FullscreenNotificationObserver fullscreen_observer;
      browser()->ToggleFullscreenModeForTab(tab, enter_fullscreen);
      fullscreen_observer.Wait();
      // Repeat ToggleFullscreenModeForTab until the correct state is entered.
      // This addresses flakiness on test bots running many fullscreen
      // tests in parallel.
    } while (retry_until_success &&
             browser()->window()->IsFullscreen() != enter_fullscreen);
    ASSERT_EQ(browser()->window()->IsFullscreen(), enter_fullscreen);
  }
}
