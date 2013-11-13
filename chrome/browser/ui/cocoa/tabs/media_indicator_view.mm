// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tabs/media_indicator_view.h"

#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/image/image.h"

class MediaIndicatorViewAnimationDelegate : public gfx::AnimationDelegate {
 public:
  MediaIndicatorViewAnimationDelegate(NSView* view,
                                      TabMediaState* mediaState,
                                      TabMediaState* animatingMediaState)
      : view_(view), mediaState_(mediaState),
        animatingMediaState_(animatingMediaState),
        doneCallbackObject_(nil), doneCallbackSelector_(nil) {}
  virtual ~MediaIndicatorViewAnimationDelegate() {}

  void SetAnimationDoneCallback(id anObject, SEL selector) {
    doneCallbackObject_ = anObject;
    doneCallbackSelector_ = selector;
  }

  virtual void AnimationEnded(const gfx::Animation* animation) OVERRIDE {
    *animatingMediaState_ = *mediaState_;
    [view_ setNeedsDisplay:YES];
    [doneCallbackObject_ performSelector:doneCallbackSelector_];
  }
  virtual void AnimationProgressed(const gfx::Animation* animation) OVERRIDE {
    [view_ setNeedsDisplay:YES];
  }
  virtual void AnimationCanceled(const gfx::Animation* animation) OVERRIDE {
    AnimationEnded(animation);
  }

 private:
  NSView* const view_;
  TabMediaState* const mediaState_;
  TabMediaState* const animatingMediaState_;

  id doneCallbackObject_;
  SEL doneCallbackSelector_;
};

@implementation MediaIndicatorView

@synthesize mediaState = mediaState_;
@synthesize animatingMediaState = animatingMediaState_;

- (id)init {
  if ((self = [super initWithFrame:NSZeroRect])) {
    mediaState_ = animatingMediaState_ = TAB_MEDIA_STATE_NONE;
    delegate_.reset(new MediaIndicatorViewAnimationDelegate(
        self, &mediaState_, &animatingMediaState_));
  }
  return self;
}

- (void)updateIndicator:(TabMediaState)mediaState {
  if (mediaState == mediaState_)
    return;

  mediaState_ = mediaState;
  animation_.reset();

  // Prepare this view if the new TabMediaState is an active one.
  if (mediaState_ != TAB_MEDIA_STATE_NONE) {
    animatingMediaState_ = mediaState_;
    NSImage* const image =
        chrome::GetTabMediaIndicatorImage(mediaState_).ToNSImage();
    NSRect frame = [self frame];
    frame.size = [image size];
    [self setFrame:frame];
    [self setImage:image];
  }

  // If the animation delegate is missing, that means animations were disabled
  // for testing; so, go directly to animating completion state.
  if (!delegate_) {
    animatingMediaState_ = mediaState_;
    return;
  }

  animation_ = chrome::CreateTabMediaIndicatorFadeAnimation(mediaState_);
  animation_->set_delegate(delegate_.get());
  animation_->Start();
}

- (void)setAnimationDoneCallbackObject:(id)anObject withSelector:(SEL)selector {
  if (delegate_)
    delegate_->SetAnimationDoneCallback(anObject, selector);
}

- (void)drawRect:(NSRect)rect {
  if (!animation_)
    return;

  double opaqueness = animation_->GetCurrentValue();
  if (mediaState_ == TAB_MEDIA_STATE_NONE)
    opaqueness = 1.0 - opaqueness;  // Fading out, not in.

  [[self image] drawInRect:[self bounds]
                  fromRect:NSZeroRect
                 operation:NSCompositeSourceOver
                  fraction:opaqueness];
}

- (void)disableAnimations {
  delegate_.reset();
}

@end
