// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_MENU_TRACKED_BUTTON_H_
#define CHROME_BROWSER_COCOA_MENU_TRACKED_BUTTON_H_
#pragma once

#import <Cocoa/Cocoa.h>

// A MenuTrackedButton is meant to be used whenever a button is placed inside
// the custom view of an NSMenuItem. If the user opens the menu in a non-sticky
// fashion (i.e. clicks, holds, and drags) and then releases the mouse over
// a MenuTrackedButton, it will |-performClick:| itself.
@interface MenuTrackedButton : NSButton {
 @private
  // If the button received a |-mouseEntered:| event. This short-circuits the
  // custom drag tracking logic.
  BOOL didEnter_;

  // Whether or not the user is in a click-drag-release event sequence. If so
  // and this receives a |-mouseUp:|, then this will click itself.
  BOOL tracking_;
}

@end

#endif  // CHROME_BROWSER_COCOA_MENU_TRACKED_BUTTON_H_
