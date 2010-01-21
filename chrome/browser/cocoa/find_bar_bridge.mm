// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/find_bar_bridge.h"

#include "base/sys_string_conversions.h"
#import "chrome/browser/cocoa/find_bar_cocoa_controller.h"

FindBarBridge::FindBarBridge()
    : find_bar_controller_(NULL) {
  cocoa_controller_.reset([[FindBarCocoaController alloc] init]);
  [cocoa_controller_ setFindBarBridge:this];
}

FindBarBridge::~FindBarBridge() {
}

void FindBarBridge::Show(bool animate) {
  [cocoa_controller_ showFindBar:(animate ? YES : NO)];
}

void FindBarBridge::Hide(bool animate) {
  [cocoa_controller_ hideFindBar:(animate ? YES : NO)];
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
  // http://crbug.com/11084
  // http://crbug.com/22036
}

void FindBarBridge::StopAnimation() {
  [cocoa_controller_ stopAnimation];
}

void FindBarBridge::RestoreSavedFocus() {
  [cocoa_controller_ restoreSavedFocus];
}
