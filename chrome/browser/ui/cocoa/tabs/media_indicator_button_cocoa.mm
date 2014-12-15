// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tabs/media_indicator_button_cocoa.h"

#include "base/logging.h"
#include "base/thread_task_runner_handle.h"
#include "content/public/browser/user_metrics.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/image/image.h"

@implementation MediaIndicatorButton

class FadeAnimationDelegate : public gfx::AnimationDelegate {
 public:
  explicit FadeAnimationDelegate(MediaIndicatorButton* button)
      : button_(button) {}
  ~FadeAnimationDelegate() override {}

 private:
  // gfx::AnimationDelegate implementation.
  void AnimationProgressed(const gfx::Animation* animation) override {
    [button_ setNeedsDisplay:YES];
  }

  void AnimationCanceled(const gfx::Animation* animation) override {
    AnimationEnded(animation);
  }

  void AnimationEnded(const gfx::Animation* animation) override {
    button_->showingMediaState_ = button_->mediaState_;
    [button_ setNeedsDisplay:YES];
    [button_->animationDoneTarget_
        performSelector:button_->animationDoneAction_];
  }

  MediaIndicatorButton* const button_;

  DISALLOW_COPY_AND_ASSIGN(FadeAnimationDelegate);
};

@synthesize showingMediaState = showingMediaState_;

- (id)init {
  if ((self = [super initWithFrame:NSZeroRect])) {
    mediaState_ = TAB_MEDIA_STATE_NONE;
    showingMediaState_ = TAB_MEDIA_STATE_NONE;
    [self setEnabled:NO];
    [super setTarget:self];
    [super setAction:@selector(handleClick:)];
  }
  return self;
}

- (void)removeFromSuperview {
  fadeAnimation_.reset();
  [super removeFromSuperview];
}

- (void)transitionToMediaState:(TabMediaState)nextState {
  if (nextState == mediaState_)
    return;

  if (nextState != TAB_MEDIA_STATE_NONE) {
    [self setImage:chrome::GetTabMediaIndicatorImage(nextState).ToNSImage()];
    affordanceImage_.reset(
        [chrome::GetTabMediaIndicatorAffordanceImage(nextState).ToNSImage()
               retain]);
  }

  if ((mediaState_ == TAB_MEDIA_STATE_AUDIO_PLAYING &&
       nextState == TAB_MEDIA_STATE_AUDIO_MUTING) ||
      (mediaState_ == TAB_MEDIA_STATE_AUDIO_MUTING &&
       nextState == TAB_MEDIA_STATE_AUDIO_PLAYING) ||
      (mediaState_ == TAB_MEDIA_STATE_AUDIO_MUTING &&
       nextState == TAB_MEDIA_STATE_NONE)) {
    // Instant user feedback: No fade animation.
    showingMediaState_ = nextState;
    fadeAnimation_.reset();
  } else {
    if (nextState == TAB_MEDIA_STATE_NONE)
      showingMediaState_ = mediaState_;  // Fading-out indicator.
    else
      showingMediaState_ = nextState;  // Fading-in to next indicator.
    // gfx::Animation requires a task runner is available for the current
    // thread.  Generally, only certain unit tests would not instantiate a task
    // runner.
    if (base::ThreadTaskRunnerHandle::IsSet()) {
      fadeAnimation_ = chrome::CreateTabMediaIndicatorFadeAnimation(nextState);
      if (!fadeAnimationDelegate_)
        fadeAnimationDelegate_.reset(new FadeAnimationDelegate(self));
      fadeAnimation_->set_delegate(fadeAnimationDelegate_.get());
      fadeAnimation_->Start();
    }
  }

  [self setEnabled:(chrome::IsTabAudioMutingFeatureEnabled() &&
                    (nextState == TAB_MEDIA_STATE_AUDIO_PLAYING ||
                     nextState == TAB_MEDIA_STATE_AUDIO_MUTING))];

  // An indicator state change should be made visible immediately, instead of
  // the user being surprised when their mouse leaves the button.
  if ([self hoverState] == kHoverStateMouseOver)
    [self setHoverState:kHoverStateNone];

  mediaState_ = nextState;

  [self setNeedsDisplay:YES];
}

- (void)setTarget:(id)aTarget {
  NOTREACHED();  // See class-level comments.
}

- (void)setAction:(SEL)anAction {
  NOTREACHED();  // See class-level comments.
}

- (void)setAnimationDoneTarget:(id)target withAction:(SEL)action {
  animationDoneTarget_ = target;
  animationDoneAction_ = action;
}

- (void)setClickTarget:(id)target withAction:(SEL)action {
  clickTarget_ = target;
  clickAction_ = action;
}

- (void)drawRect:(NSRect)dirtyRect {
  NSImage* image = ([self hoverState] == kHoverStateNone || ![self isEnabled]) ?
      [self image] : affordanceImage_.get();
  if (!image)
    return;
  NSRect imageRect = NSZeroRect;
  imageRect.size = [image size];
  NSRect destRect = [self bounds];
  destRect.origin.y =
      floor((NSHeight(destRect) / 2) - (NSHeight(imageRect) / 2));
  destRect.size = imageRect.size;
  double opaqueness =
      fadeAnimation_ ? fadeAnimation_->GetCurrentValue() : 1.0;
  if (mediaState_ == TAB_MEDIA_STATE_NONE)
    opaqueness = 1.0 - opaqueness;  // Fading out, not in.
  [image drawInRect:destRect
           fromRect:imageRect
          operation:NSCompositeSourceOver
           fraction:opaqueness
     respectFlipped:YES
              hints:nil];
}

// When disabled, the superview should receive all mouse events.
- (NSView*)hitTest:(NSPoint)aPoint {
  if ([self isEnabled] && ![self isHidden])
    return [super hitTest:aPoint];
  else
    return nil;
}

- (void)handleClick:(id)sender {
  using base::UserMetricsAction;

  if (mediaState_ == TAB_MEDIA_STATE_AUDIO_PLAYING)
    content::RecordAction(UserMetricsAction("MediaIndicatorButton_Mute"));
  else if (mediaState_ == TAB_MEDIA_STATE_AUDIO_MUTING)
    content::RecordAction(UserMetricsAction("MediaIndicatorButton_Unmute"));
  else
    NOTREACHED();

  [clickTarget_ performSelector:clickAction_ withObject:self];
}

@end
