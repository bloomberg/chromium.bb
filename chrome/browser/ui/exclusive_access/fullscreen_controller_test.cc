// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/exclusive_access/fullscreen_controller_test.h"

#include "base/command_line.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/keyboard_codes.h"

using content::WebContents;

const char FullscreenControllerTest::kFullscreenMouseLockHTML[] =
    "/fullscreen_mouselock/fullscreen_mouselock.html";

void FullscreenControllerTest::RequestToLockMouse(
    bool user_gesture,
    bool last_unlocked_by_target) {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  MouseLockController* mouse_lock_controller =
      GetExclusiveAccessManager()->mouse_lock_controller();
  mouse_lock_controller->set_fake_mouse_lock_for_test(true);
  browser()->RequestToLockMouse(tab, user_gesture,
      last_unlocked_by_target);
  mouse_lock_controller->set_fake_mouse_lock_for_test(false);
}

FullscreenController* FullscreenControllerTest::GetFullscreenController() {
  return GetExclusiveAccessManager()->fullscreen_controller();
}

ExclusiveAccessManager* FullscreenControllerTest::GetExclusiveAccessManager() {
  return browser()->exclusive_access_manager();
}

void FullscreenControllerTest::LostMouseLock() {
  browser()->LostMouseLock();
}

bool FullscreenControllerTest::SendEscapeToFullscreenController() {
  content::NativeWebKeyboardEvent event(
      blink::WebInputEvent::KeyDown, blink::WebInputEvent::NoModifiers,
      blink::WebInputEvent::TimeStampForTesting);
  event.windowsKeyCode = ui::VKEY_ESCAPE;
  return GetExclusiveAccessManager()->HandleUserKeyPress(event);
}

bool FullscreenControllerTest::IsFullscreenForBrowser() {
  return GetFullscreenController()->IsFullscreenForBrowser();
}

bool FullscreenControllerTest::IsWindowFullscreenForTabOrPending() {
  return GetFullscreenController()->IsWindowFullscreenForTabOrPending();
}

ExclusiveAccessBubbleType
FullscreenControllerTest::GetExclusiveAccessBubbleType() {
  return GetExclusiveAccessManager()->GetExclusiveAccessExitBubbleType();
}

bool FullscreenControllerTest::IsFullscreenBubbleDisplayed() {
  return GetExclusiveAccessBubbleType() != EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE;
}

void FullscreenControllerTest::GoBack() {
  content::TestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents(), 1);
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  observer.Wait();
}

void FullscreenControllerTest::Reload() {
  content::TestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents(), 1);
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  observer.Wait();
}

void FullscreenControllerTest::SetPrivilegedFullscreen(bool is_privileged) {
  GetFullscreenController()->SetPrivilegedFullscreenForTesting(is_privileged);
}
