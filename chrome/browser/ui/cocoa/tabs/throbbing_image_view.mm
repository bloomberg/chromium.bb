// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tabs/throbbing_image_view.h"

#include "ui/base/animation/animation_delegate.h"

class ThrobbingImageViewAnimationDelegate : public ui::AnimationDelegate {
 public:
  ThrobbingImageViewAnimationDelegate(NSView* view) : view_(view) {}
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE {
    [view_ setNeedsDisplay:YES];
  }
 private:
  NSView* view_;  // weak
};

@implementation ThrobbingImageView

- (id)initWithFrame:(NSRect)rect
    backgroundImage:(NSImage*)backgroundImage
         throbImage:(NSImage*)throbImage
         durationMS:(int)durationMS {
  if ((self = [super initWithFrame:rect])) {
    backgroundImage_.reset([backgroundImage retain]);
    throbImage_.reset([throbImage retain]);

    delegate_.reset(new ThrobbingImageViewAnimationDelegate(self));
    throbAnimation_.reset(new ui::ThrobAnimation(delegate_.get()));
    throbAnimation_->SetThrobDuration(durationMS);
    throbAnimation_->StartThrobbing(-1);
  }
  return self;
}

- (void)setTweenType:(ui::Tween::Type)type {
  throbAnimation_->SetTweenType(type);
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
  [throbImage_ drawInRect:[self bounds]
                 fromRect:NSZeroRect
                operation:NSCompositeSourceOver
                 fraction:throbAnimation_->GetCurrentValue()];
}

@end
