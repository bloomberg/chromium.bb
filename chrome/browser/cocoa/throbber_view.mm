// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/throbber_view.h"

#include "base/logging.h"

static const float kAnimationIntervalSeconds = 0.03;  // 30ms, same as windows

@interface ThrobberView(PrivateMethods)
- (id)initWithFrame:(NSRect)frame delegate:(id<ThrobberDataDelegate>)delegate;
- (void)animate;
@end

@protocol ThrobberDataDelegate <NSObject>
// Is the current frame the last frame of the animation?
- (BOOL)animationIsComplete;

// Draw the current frame into the current graphics context.
- (void)drawFrameInRect:(NSRect)rect;

// Update the frame counter.
- (void)advanceFrame;
@end

@interface ThrobberFilmstripDelegate : NSObject
                                       <ThrobberDataDelegate> {
  scoped_nsobject<NSImage> image_;
  unsigned int numFrames_;  // Number of frames in this animation.
  unsigned int animationFrame_;  // Current frame of the animation,
                                 // [0..numFrames_)
}

- (id)initWithImage:(NSImage*)image;

@end

@implementation ThrobberFilmstripDelegate

// Stores the internal representation of the image from |image|. We use
// CoreImage for speed (though this doesn't seem to help perf issues). We
// validate that the image is of the appropriate ratio.
- (id)initWithImage:(NSImage*)image {
  if ((self = [super init])) {
    // Reset the animation counter so there's no chance we are off the end.
    animationFrame_ = 0;

    // Ensure that the height divides evenly into the width. Cache the
    // number of frames in the animation for later.
    NSSize imageSize = [image size];
    DCHECK(imageSize.height && imageSize.width);
    if (!imageSize.height)
      return nil;
    DCHECK((int)imageSize.width % (int)imageSize.height == 0);
    numFrames_ = (int)imageSize.width / (int)imageSize.height;
    DCHECK(numFrames_);
    image_.reset([image retain]);
  }
  return self;
}

- (BOOL)animationIsComplete {
  return NO;
}

- (void)drawFrameInRect:(NSRect)rect {
  float imageDimension = [image_ size].height;
  float xOffset = animationFrame_ * imageDimension;
  NSRect sourceImageRect =
      NSMakeRect(xOffset, 0, imageDimension, imageDimension);
  [image_ drawInRect:rect
            fromRect:sourceImageRect
           operation:NSCompositeSourceOver
            fraction:1.0];
}

- (void)advanceFrame {
  animationFrame_ = ++animationFrame_ % numFrames_;
}

@end

@interface ThrobberToastDelegate : NSObject
                                   <ThrobberDataDelegate> {
  scoped_nsobject<NSImage> image1_;
  scoped_nsobject<NSImage> image2_;
  NSSize image1Size_;
  NSSize image2Size_;
  int animationFrame_;  // Current frame of the animation,
}

- (id)initWithImage1:(NSImage*)image1 image2:(NSImage*)image2;

@end

@implementation ThrobberToastDelegate

- (id)initWithImage1:(NSImage*)image1 image2:(NSImage*)image2 {
  if ((self = [super init])) {
    image1_.reset([image1 retain]);
    image2_.reset([image2 retain]);
    image1Size_ = [image1 size];
    image2Size_ = [image2 size];
    animationFrame_ = 0;
  }
  return self;
}

- (BOOL)animationIsComplete {
  if (animationFrame_ >= image1Size_.height + image2Size_.height)
    return YES;

  return NO;
}

// From [0..image1Height) we draw image1, at image1Height we draw nothing, and
// from [image1Height+1..image1Hight+image2Height] we draw the second image.
- (void)drawFrameInRect:(NSRect)rect {
  NSImage* image = nil;
  NSSize srcSize;
  NSRect destRect;

  if (animationFrame_ < image1Size_.height) {
    image = image1_.get();
    srcSize = image1Size_;
    destRect = NSMakeRect(0, -animationFrame_,
                          image1Size_.width, image1Size_.height);
  } else if (animationFrame_ == image1Size_.height) {
    // nothing; intermediate blank frame
  } else {
    image = image2_.get();
    srcSize = image2Size_;
    destRect = NSMakeRect(0, animationFrame_ -
                                 (image1Size_.height + image2Size_.height),
                          image2Size_.width, image2Size_.height);
  }

  if (image) {
    NSRect sourceImageRect =
        NSMakeRect(0, 0, srcSize.width, srcSize.height);
    [image drawInRect:destRect
             fromRect:sourceImageRect
            operation:NSCompositeSourceOver
             fraction:1.0];
  }
}

- (void)advanceFrame {
  ++animationFrame_;
}

@end

// A very simple object that is the target for the animation timer so that
// the view isn't. We do this to avoid retain cycles as the timer
// retains its target.
@interface ThrobberTimerTarget : NSObject {
 @private
  ThrobberView* throbber_;  // Weak, owns us
}
- (id)initWithThrobber:(ThrobberView*)view;
@end

@implementation ThrobberTimerTarget
- (id)initWithThrobber:(ThrobberView*)view {
  if ((self = [super init])) {
    throbber_ = view;
  }
  return self;
}

- (void)animate:(NSTimer*)timer {
  [throbber_ animate];
}
@end

@implementation ThrobberView

+ (id)filmstripThrobberViewWithFrame:(NSRect)frame
                               image:(NSImage*)image {
  ThrobberFilmstripDelegate* delegate =
      [[[ThrobberFilmstripDelegate alloc] initWithImage:image] autorelease];
  if (!delegate)
    return nil;

  return [[[ThrobberView alloc] initWithFrame:frame
                                     delegate:delegate] autorelease];
}

+ (id)toastThrobberViewWithFrame:(NSRect)frame
                     beforeImage:(NSImage*)beforeImage
                      afterImage:(NSImage*)afterImage {
  ThrobberToastDelegate* delegate =
      [[[ThrobberToastDelegate alloc] initWithImage1:beforeImage
                                              image2:afterImage] autorelease];
  if (!delegate)
    return nil;

  return [[[ThrobberView alloc] initWithFrame:frame
                                     delegate:delegate] autorelease];
}

- (id)initWithFrame:(NSRect)frame delegate:(id<ThrobberDataDelegate>)delegate {
  if ((self = [super initWithFrame:frame])) {
    dataDelegate_ = [delegate retain];

    // Start a timer for the animation frames.
    target_.reset([[ThrobberTimerTarget alloc] initWithThrobber:self]);
    timer_ = [NSTimer scheduledTimerWithTimeInterval:kAnimationIntervalSeconds
                                              target:target_.get()
                                            selector:@selector(animate:)
                                            userInfo:nil
                                             repeats:YES];
  }
  return self;
}

- (void)dealloc {
  [dataDelegate_ release];
  if (timer_)
    [timer_ invalidate];

  [super dealloc];
}

- (void)removeFromSuperview {
  [timer_ invalidate];
  timer_ = nil;

  [super removeFromSuperview];
}

// Called when the ThrobberTimerTarget gets tickled by our timer. Advance the
// frame, dirty the display, and kill the timer when it's no longer needed.
- (void)animate {
  [dataDelegate_ advanceFrame];
  [self setNeedsDisplay:YES];

  if ([dataDelegate_ animationIsComplete]) {
    [timer_ invalidate];
    timer_ = nil;
  }
}

// Overridden to draw the appropriate frame in the image strip.
- (void)drawRect:(NSRect)rect {
  [dataDelegate_ drawFrameInRect:[self bounds]];
}

@end
