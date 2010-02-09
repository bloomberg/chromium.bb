// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_NOTIFICATIONS_BALLOON_VIEW_H_
#define CHROME_BROWSER_COCOA_NOTIFICATIONS_BALLOON_VIEW_H_

#import <Cocoa/Cocoa.h>

// This view class draws a frame around the HTML contents of a
// notification balloon.
@interface BalloonViewCocoa : NSView {
}
@end

// This view class draws the shelf of a notification balloon,
// containing the controls.
@interface BalloonShelfViewCocoa : NSView {
}
@end

// This view draws a button with the shelf of the balloon.
@interface BalloonButtonCell : NSButtonCell {
}
- (void)setTextColor:(NSColor*)color;
@end

#endif  // CHROME_BROWSER_COCOA_NOTIFICATIONS_BALLOON_VIEW_H_
