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

void FindBarBridge::Show() {
  [cocoa_controller_ showFindBar:YES];  // with animation.
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
  // TODO(rohitrao): Beep beep, beep beep, Yeah!
}

bool FindBarBridge::IsFindBarVisible() {
  return [cocoa_controller_ isFindBarVisible] ? true : false;
}

void FindBarBridge::MoveWindowIfNecessary(const gfx::Rect& selection_rect,
                                          bool no_redraw) {
  // http://crbug.com/11084
}

void FindBarBridge::StopAnimation() {
  [cocoa_controller_ stopAnimation];
}

gfx::Rect FindBarBridge::GetDialogPosition(gfx::Rect avoid_overlapping_rect) {
  // http://crbug.com/22036
  return gfx::Rect();
}

void FindBarBridge::SetDialogPosition(const gfx::Rect& new_pos,
                                      bool no_redraw) {
  // TODO(rohitrao): Do something useful here.  For now, just show the findbar.
  // http://crbug.com/22036
  [cocoa_controller_ showFindBar:NO];  // Do not animate.
}

void FindBarBridge::RestoreSavedFocus() {
  [cocoa_controller_ restoreSavedFocus];
}
