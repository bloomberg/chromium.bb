// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_ANIMATABLE_IMAGE_H_
#define CHROME_BROWSER_UI_COCOA_ANIMATABLE_IMAGE_H_
#pragma once

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>

#include "base/scoped_nsobject.h"

// This class helps animate an NSImage's frame and opacity. It works by creating
// a blank NSWindow in the size specified and giving it a layer on which the
// image can be animated. Clients are free to embed this object as a child
// window for easier window management. This class will clean itself up when
// the animation has finished. Clients that install this as a child window
// should listen for the NSWindowWillCloseNotification to perform any additional
// cleanup.
@interface AnimatableImage : NSWindow {
 @private
  // The image to animate.
  scoped_nsobject<NSImage> image_;

  // The frame of the image before and after the animation. This is in this
  // window's coordinate system.
  CGRect startFrame_;
  CGRect endFrame_;

  // Opacity values for the animation.
  CGFloat startOpacity_;
  CGFloat endOpacity_;

  // The amount of time it takes to animate the image.
  CGFloat duration_;
}

@property(nonatomic) CGRect startFrame;
@property(nonatomic) CGRect endFrame;
@property(nonatomic) CGFloat startOpacity;
@property(nonatomic) CGFloat endOpacity;
@property(nonatomic) CGFloat duration;

// Designated initializer. Do not use any other NSWindow initializers. Creates
// but does not show the blank animation window of the given size. The
// |animationFrame| should usually be big enough to contain the |startFrame|
// and |endFrame| properties of the animation.
- (id)initWithImage:(NSImage*)image
     animationFrame:(NSRect)animationFrame;

// Begins the animation.
- (void)startAnimation;

@end

#endif  // CHROME_BROWSER_UI_COCOA_ANIMATABLE_IMAGE_H_
