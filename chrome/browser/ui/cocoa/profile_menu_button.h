// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PROFILE_MENU_BUTTON_H_
#define CHROME_BROWSER_UI_COCOA_PROFILE_MENU_BUTTON_H_
#pragma once

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"

// PopUp button that shows the multiprofile menu.
@interface ProfileMenuButton : NSPopUpButton {
 @private
  BOOL shouldShowProfileDisplayName_;
  scoped_nsobject<NSTextFieldCell> textFieldCell_;

  scoped_nsobject<NSImage> cachedTabImage_;
  // Cache the various button states when creating |cachedTabImage_|. If
  // any of these states change then the cached image is invalidated.
  BOOL cachedTabImageIsPressed_;
}

@property(assign,nonatomic) BOOL shouldShowProfileDisplayName;
@property(assign,nonatomic) NSString* profileDisplayName;

// Gets the size of the control that would display all its contents.
- (NSSize)desiredControlSize;
// Gets the minimum size that the control should be resized to.
- (NSSize)minControlSize;

// Public for testing.
- (void)   mouseDown:(NSEvent*)event
  withShowMenuTarget:(id)target;

@end

#endif  // CHROME_BROWSER_UI_COCOA_PROFILE_MENU_BUTTON_H_
