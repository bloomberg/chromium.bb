// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_SPRITE_VIEW_H_
#define CHROME_BROWSER_UI_COCOA_SPRITE_VIEW_H_

#import <Cocoa/Cocoa.h>

// A class that knows how to repeatedly animate sprites from an image containing
// the sprites in the form of a horizontal strip.
@interface SpriteView : NSView

// |image| contains the sprites in the form of a horizontal strip.
// |spriteWidth| specifies the width of each sprite.
// The sprite's height is the same as the height of |image|.
// Each sprite will be displayed for |duration| seconds.
- (instancetype)initWithImage:(NSImage*)image
                        width:(CGFloat)spriteWidth
                     duration:(CGFloat)duration;

// Same as above, but defaults to a square sprite where the sprite's
// width and height is the same as the height of |image|.
// Each sprite will be displayed for 30ms.
- (instancetype)initWithImage:(NSImage*)image;

@end

#endif  // CHROME_BROWSER_UI_COCOA_TABS_THROBBER_VIEW_H_
