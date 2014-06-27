// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_SPRITE_VIEW_H_
#define CHROME_BROWSER_UI_COCOA_SPRITE_VIEW_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"

@class CAKeyframeAnimation;

// A class that knows how to repeatedly animate sprites from an image containing
// the sprites in the form of a horizontal strip.
@interface SpriteView : NSView {
 @private
  base::scoped_nsobject<CAKeyframeAnimation> spriteAnimation_;
  CALayer* imageLayer_;
}

// |image| contains square sprites in a horizontal strip.
// The sprites will be animated, with each sprite shown for 30ms.
// It is OK to pass in a single sprite (a square image), in which case there
// will be no animation.
- (void)setImage:(NSImage*)image;

// Same as above, with a toast animation to transition to the new image.
// The old image will animate to the bottom, and then the new image will
// animate back up to position.
- (void)setImage:(NSImage*)image withToastAnimation:(BOOL)animate;

@end

#endif  // CHROME_BROWSER_UI_COCOA_TABS_THROBBER_VIEW_H_
