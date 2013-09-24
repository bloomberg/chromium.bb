// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tabs/throbbing_image_view.h"

#include "chrome/browser/ui/tabs/tab_utils.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/animation_delegate.h"

class ThrobbingImageViewAnimationDelegate : public gfx::AnimationDelegate {
 public:
  ThrobbingImageViewAnimationDelegate(NSView* view) : view_(view) {}
  virtual void AnimationProgressed(const gfx::Animation* animation) OVERRIDE {
    [view_ setNeedsDisplay:YES];
  }
 private:
  NSView* view_;  // weak
};

@implementation ThrobbingImageView

- (id)initWithFrame:(NSRect)rect
       backgroundImage:(NSImage*)backgroundImage
            throbImage:(NSImage*)throbImage
         throbPosition:(ThrobPosition)throbPosition
    animationContainer:(gfx::AnimationContainer*)animationContainer {
  if ((self = [super initWithFrame:rect])) {
    backgroundImage_.reset([backgroundImage retain]);
    throbImage_.reset([throbImage retain]);

    delegate_.reset(new ThrobbingImageViewAnimationDelegate(self));

    throbAnimation_ = chrome::CreateTabRecordingIndicatorAnimation();
    throbAnimation_->set_delegate(delegate_.get());
    throbAnimation_->SetContainer(animationContainer);
    throbAnimation_->Start();

    throbPosition_ = throbPosition;
  }
  return self;
}

- (void)dealloc {
  throbAnimation_->Stop();
  [super dealloc];
}

- (void)drawRect:(NSRect)rect {
  [backgroundImage_ drawInRect:[self bounds]
                      fromRect:NSZeroRect
                     operation:NSCompositeSourceOver
                      fraction:1];

  NSRect b = [self bounds];
  NSRect throbImageBounds;
  if (throbPosition_ == kThrobPositionBottomRight) {
    NSSize throbImageSize = [throbImage_ size];
    throbImageBounds.origin = b.origin;
    throbImageBounds.origin.x += NSWidth(b) - throbImageSize.width;
    throbImageBounds.size = throbImageSize;
  } else {
    throbImageBounds = b;
  }
  [throbImage_ drawInRect:throbImageBounds
                 fromRect:NSZeroRect
                operation:NSCompositeSourceOver
                 fraction:throbAnimation_->GetCurrentValue()];
}

@end
