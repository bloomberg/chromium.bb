// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TABS_THROBBING_IMAGE_VIEW_H_
#define CHROME_BROWSER_UI_COCOA_TABS_THROBBING_IMAGE_VIEW_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "ui/base/animation/throb_animation.h"

class ThrobbingImageViewAnimationDelegate;

@interface ThrobbingImageView : NSView {
 @protected
  scoped_nsobject<NSImage> backgroundImage_;
  scoped_nsobject<NSImage> throbImage_;
  scoped_ptr<ui::ThrobAnimation> throbAnimation_;

 @private
  scoped_ptr<ThrobbingImageViewAnimationDelegate> delegate_;
}

- (id)initWithFrame:(NSRect)rect
    backgroundImage:(NSImage*)backgroundImage
         throbImage:(NSImage*)throbImage
         durationMS:(int)durationMS;

- (void)setTweenType:(ui::Tween::Type)type;

@end

#endif  // CHROME_BROWSER_UI_COCOA_TABS_THROBBING_IMAGE_VIEW_H_
