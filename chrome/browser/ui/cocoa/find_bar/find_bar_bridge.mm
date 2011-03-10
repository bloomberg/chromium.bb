// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#include "chrome/browser/ui/cocoa/find_bar/find_bar_bridge.h"

#include "base/sys_string_conversions.h"
#import "chrome/browser/ui/cocoa/find_bar/find_bar_cocoa_controller.h"

// static
bool FindBarBridge::disable_animations_during_testing_ = false;

FindBarBridge::FindBarBridge()
    : find_bar_controller_(NULL) {
  cocoa_controller_ = [[FindBarCocoaController alloc] init];
  [cocoa_controller_ setFindBarBridge:this];
}

FindBarBridge::~FindBarBridge() {
  [cocoa_controller_ release];
}

void FindBarBridge::SetFindBarController(
    FindBarController* find_bar_controller) {
  find_bar_controller_ = find_bar_controller;
}

FindBarController* FindBarBridge::GetFindBarController() const {
  return find_bar_controller_;
}

FindBarTesting* FindBarBridge::GetFindBarTesting() {
  return this;
}

void FindBarBridge::Show(bool animate) {
  bool really_animate = animate && !disable_animations_during_testing_;
  [cocoa_controller_ showFindBar:(really_animate ? YES : NO)];
}

void FindBarBridge::Hide(bool animate) {
  bool really_animate = animate && !disable_animations_during_testing_;
  [cocoa_controller_ hideFindBar:(really_animate ? YES : NO)];
}

void FindBarBridge::SetFocusAndSelection() {
  [cocoa_controller_ setFocusAndSelection];
}

void FindBarBridge::ClearResults(const FindNotificationDetails& results) {
  [cocoa_controller_ clearResults:results];
}

void FindBarBridge::SetFindText(const string16& find_text) {
  [cocoa_controller_ setFindText:base::SysUTF16ToNSString(find_text)];
}

void FindBarBridge::UpdateUIForFindResult(const FindNotificationDetails& result,
                                          const string16& find_text) {
  [cocoa_controller_ updateUIForFindResult:result withText:find_text];
}

void FindBarBridge::AudibleAlert() {
  // Beep beep, beep beep, Yeah!
  NSBeep();
}

bool FindBarBridge::IsFindBarVisible() {
  return [cocoa_controller_ isFindBarVisible] ? true : false;
}

void FindBarBridge::MoveWindowIfNecessary(const gfx::Rect& selection_rect,
                                          bool no_redraw) {
  // See FindBarCocoaController moveFindBarToAvoidRect.
}

void FindBarBridge::StopAnimation() {
  [cocoa_controller_ stopAnimation];
}

void FindBarBridge::RestoreSavedFocus() {
  [cocoa_controller_ restoreSavedFocus];
}

bool FindBarBridge::GetFindBarWindowInfo(gfx::Point* position,
                                         bool* fully_visible) {
  NSWindow* window = [[cocoa_controller_ view] window];
  bool window_visible = [window isVisible] ? true : false;
  if (position) {
    if (window_visible)
      *position = [cocoa_controller_ findBarWindowPosition];
    else
      *position = gfx::Point(0, 0);
  }
  if (fully_visible) {
    *fully_visible = window_visible &&
        [cocoa_controller_ isFindBarVisible] &&
        ![cocoa_controller_ isFindBarAnimating];
  }
  return window_visible;
}

string16 FindBarBridge::GetFindText() {
  // This function is currently only used in Windows and Linux specific browser
  // tests (testing prepopulate values that Mac's don't rely on), but if we add
  // more tests that are non-platform specific, we need to flesh out this
  // function.
  NOTIMPLEMENTED();
  return string16();
}

string16 FindBarBridge::GetFindSelectedText() {
  // This function is currently only used in Windows and Linux specific browser
  // tests (testing prepopulate values that Mac's don't rely on), but if we add
  // more tests that are non-platform specific, we need to flesh out this
  // function.
  NOTIMPLEMENTED();
  return string16();
}

string16 FindBarBridge::GetMatchCountText() {
  // This function is currently only used in Windows and Linux specific browser
  // tests (testing prepopulate values that Mac's don't rely on), but if we add
  // more tests that are non-platform specific, we need to flesh out this
  // function.
  NOTIMPLEMENTED();
  return string16();
}
