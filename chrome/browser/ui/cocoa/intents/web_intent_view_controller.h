// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_INTENTS_WEB_INTENT_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_INTENTS_WEB_INTENT_VIEW_CONTROLLER_H_

#include <Cocoa/Cocoa.h>

// This protocol declares methods that all web intent view controllers should
// implement.
@protocol WebIntentViewController

// Gets the size of the view for the given width. |innerWidth| and the returned
// size does not include space for padding around the edge of the window.
- (NSSize)minimumSizeForInnerWidth:(CGFloat)innerWidth;

// Layout subviews in the given frame.
- (void)layoutSubviewsWithinFrame:(NSRect)innerFrame;

@end

#endif  // CHROME_BROWSER_UI_COCOA_INTENTS_WEB_INTENT_VIEW_CONTROLLER_H_
