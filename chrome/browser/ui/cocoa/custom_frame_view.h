// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

// CustomFrameView is a class whose methods we swizzle into NSGrayFrame
// on 10.7 and below, or NSThemeFrame on 10.8 and above, so that we can
// support custom frame drawing.
// This class is never to be instantiated on its own.

@interface NSView (CustomFrameView)

// Returns where the fullscreen button's origin should be positioned in window
// coordinates.
// We swizzle NSThemeFrame's implementation to center it vertically in the
// tabstrip (if there is a tabstrip), and to shift it to the left of the
// old-style avatar icon if necessary.
- (NSPoint)_fullScreenButtonOrigin;

@end
