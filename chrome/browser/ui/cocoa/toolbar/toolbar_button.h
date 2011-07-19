// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TOOLBAR_TOOLBAR_BUTTON_H_
#define CHROME_BROWSER_UI_COCOA_TOOLBAR_TOOLBAR_BUTTON_H_
#pragma once

#import <Cocoa/Cocoa.h>

// NSButton subclass which handles middle mouse clicking.

@interface ToolbarButton : NSButton {
 @protected
  // YES when middle mouse clicks should be handled.
  BOOL handleMiddleClick_;

  // YES when a middle mouse click is being handled. This is set to YES by an
  // NSOtherMouseDown event, and NO by an NSOtherMouseUp event. While this is
  // YES, other mouse button events should be ignored.
  BOOL handlingMiddleClick_;
}

// Whether or not to handle the mouse middle click events.
@property(assign, nonatomic) BOOL handleMiddleClick;

@end

@interface ToolbarButton (ExposedForTesting)
- (BOOL)shouldHandleEvent:(NSEvent*)theEvent;
@end

#endif  // CHROME_BROWSER_UI_COCOA_TOOLBAR_TOOLBAR_BUTTON_H_
