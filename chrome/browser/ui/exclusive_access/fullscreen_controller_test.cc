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
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_navigation_observer.h"

using content::WebContents;

const char FullscreenControllerTest::kFullscreenMouseLockHTML[] =
    "files/fullscreen_mouselock/fullscreen_mouselock.html";

void FullscreenControllerTest::RequestToLockMouse(
    bool user_gesture,
    bool last_unlocked_by_target) {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  browser()->RequestToLockMouse(tab, user_gesture,
      last_unlocked_by_target);
}

void FullscreenControllerTest::LostMouseLock() {
  browser()->LostMouseLock();
}

bool FullscreenControllerTest::SendEscapeToFullscreenController() {
  return browser()->fullscreen_controller()->HandleUserPressedEscape();
}

bool FullscreenControllerTest::IsFullscreenForBrowser() {
  return browser()->fullscreen_controller()->IsFullscreenForBrowser();
}

bool FullscreenControllerTest::IsWindowFullscreenForTabOrPending() {
  return browser()->fullscreen_controller()->
      IsWindowFullscreenForTabOrPending();
}

bool FullscreenControllerTest::IsMouseLockPermissionRequested() {
  ExclusiveAccessBubbleType type =
      browser()->fullscreen_controller()->GetExclusiveAccessBubbleType();
  bool mouse_lock = false;
  exclusive_access_bubble::PermissionRequestedByType(type, NULL, &mouse_lock);
  return mouse_lock;
}

bool FullscreenControllerTest::IsFullscreenPermissionRequested() {
  ExclusiveAccessBubbleType type =
      browser()->fullscreen_controller()->GetExclusiveAccessBubbleType();
  bool fullscreen = false;
  exclusive_access_bubble::PermissionRequestedByType(type, &fullscreen, NULL);
  return fullscreen;
}

ExclusiveAccessBubbleType
FullscreenControllerTest::GetExclusiveAccessBubbleType() {
  return browser()->fullscreen_controller()->GetExclusiveAccessBubbleType();
}

bool FullscreenControllerTest::IsFullscreenBubbleDisplayed() {
  ExclusiveAccessBubbleType type =
      browser()->fullscreen_controller()->GetExclusiveAccessBubbleType();
  return type != EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE;
}

bool FullscreenControllerTest::IsFullscreenBubbleDisplayingButtons() {
  ExclusiveAccessBubbleType type =
      browser()->fullscreen_controller()->GetExclusiveAccessBubbleType();
  return exclusive_access_bubble::ShowButtonsForType(type);
}

void FullscreenControllerTest::AcceptCurrentFullscreenOrMouseLockRequest() {
  browser()->fullscreen_controller()->OnAcceptFullscreenPermission();
}

void FullscreenControllerTest::DenyCurrentFullscreenOrMouseLockRequest() {
  browser()->fullscreen_controller()->OnDenyFullscreenPermission();
}

void FullscreenControllerTest::GoBack() {
  content::TestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents(), 1);
  chrome::GoBack(browser(), CURRENT_TAB);
  observer.Wait();
}

void FullscreenControllerTest::Reload() {
  content::TestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents(), 1);
  chrome::Reload(browser(), CURRENT_TAB);
  observer.Wait();
}

void FullscreenControllerTest::SetPrivilegedFullscreen(bool is_privileged) {
  browser()->fullscreen_controller()->
      SetPrivilegedFullscreenForTesting(is_privileged);
}
