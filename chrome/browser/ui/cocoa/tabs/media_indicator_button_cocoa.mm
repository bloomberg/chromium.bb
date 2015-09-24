// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tabs/media_indicator_button_cocoa.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/thread_task_runner_handle.h"
#import "chrome/browser/ui/cocoa/tabs/tab_view.h"
#include "content/public/browser/user_metrics.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/image/image.h"

namespace {

// The minimum required click-to-select area of an inactive tab before allowing
// the click-to-mute functionality to be enabled.  This value is in terms of
// some percentage of the MediaIndicatorButton's width.  See comments in the
// updateEnabledForMuteToggle method.
const int kMinMouseSelectableAreaPercent = 250;

}  // namespace

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
    [self
        setImage:chrome::GetTabMediaIndicatorImage(nextState, 0).ToNSImage()];
    affordanceImage_.reset(
        [chrome::GetTabMediaIndicatorAffordanceImage(nextState, 0)
                .ToNSImage() retain]);
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

  mediaState_ = nextState;

  [self updateEnabledForMuteToggle];

  // An indicator state change should be made visible immediately, instead of
  // the user being surprised when their mouse leaves the button.
  if ([self hoverState] == kHoverStateMouseOver)
    [self setHoverState:kHoverStateNone];

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

- (void)mouseDown:(NSEvent*)theEvent {
  // Do not handle this left-button mouse event if any modifier keys are being
  // held down.  Instead, the Tab should react (e.g., selection or drag start).
  if ([theEvent modifierFlags] & NSDeviceIndependentModifierFlagsMask) {
    [self setHoverState:kHoverStateNone];  // Turn off hover.
    [[self nextResponder] mouseDown:theEvent];
    return;
  }
  [super mouseDown:theEvent];
}

- (void)mouseEntered:(NSEvent*)theEvent {
  // If any modifier keys are being held down, do not turn on hover.
  if ([theEvent modifierFlags] & NSDeviceIndependentModifierFlagsMask) {
    [self setHoverState:kHoverStateNone];
    return;
  }
  [super mouseEntered:theEvent];
}

- (void)mouseMoved:(NSEvent*)theEvent {
  // If any modifier keys are being held down, turn off hover.
  if ([theEvent modifierFlags] & NSDeviceIndependentModifierFlagsMask) {
    [self setHoverState:kHoverStateNone];
    return;
  }
  [super mouseMoved:theEvent];
}

- (void)rightMouseDown:(NSEvent*)theEvent {
  // All right-button mouse events should be handled by the Tab.
  [self setHoverState:kHoverStateNone];  // Turn off hover.
  [[self nextResponder] rightMouseDown:theEvent];
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

- (void)updateEnabledForMuteToggle {
  BOOL enable = chrome::AreExperimentalMuteControlsEnabled() &&
      (mediaState_ == TAB_MEDIA_STATE_AUDIO_PLAYING ||
       mediaState_ == TAB_MEDIA_STATE_AUDIO_MUTING);

  // If the tab is not the currently-active tab, make sure it is wide enough
  // before enabling click-to-mute.  This ensures that there is enough click
  // area for the user to activate a tab rather than unintentionally muting it.
  TabView* const tabView = base::mac::ObjCCast<TabView>([self superview]);
  if (enable && tabView && ([tabView state] != NSOnState)) {
    const int requiredWidth =
        NSWidth([self frame]) * kMinMouseSelectableAreaPercent / 100;
    enable = ([tabView widthOfLargestSelectableRegion] >= requiredWidth);
  }

  [self setEnabled:enable];
}

@end
