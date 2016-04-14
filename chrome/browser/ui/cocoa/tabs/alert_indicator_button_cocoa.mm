// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tabs/alert_indicator_button_cocoa.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/macros.h"
#include "base/thread_task_runner_handle.h"
#import "chrome/browser/ui/cocoa/tabs/tab_view.h"
#include "content/public/browser/user_metrics.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/image/image.h"

namespace {

// The minimum required click-to-select area of an inactive tab before allowing
// the click-to-mute functionality to be enabled.  This value is in terms of
// some percentage of the AlertIndicatorButton's width.  See comments in the
// updateEnabledForMuteToggle method.
const int kMinMouseSelectableAreaPercent = 250;

}  // namespace

@implementation AlertIndicatorButton

class FadeAnimationDelegate : public gfx::AnimationDelegate {
 public:
  explicit FadeAnimationDelegate(AlertIndicatorButton* button)
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
    button_->showingAlertState_ = button_->alertState_;
    [button_ setNeedsDisplay:YES];
    [button_->animationDoneTarget_
        performSelector:button_->animationDoneAction_];
  }

  AlertIndicatorButton* const button_;

  DISALLOW_COPY_AND_ASSIGN(FadeAnimationDelegate);
};

@synthesize showingAlertState = showingAlertState_;

- (id)init {
  if ((self = [super initWithFrame:NSZeroRect])) {
    alertState_ = TabAlertState::NONE;
    showingAlertState_ = TabAlertState::NONE;
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

- (void)updateIconForState:(TabAlertState)aState {
  if (aState != TabAlertState::NONE) {
    TabView* const tabView = base::mac::ObjCCast<TabView>([self superview]);
    SkColor iconColor = [tabView closeButtonColor];
    NSImage* tabIndicatorImage =
        chrome::GetTabAlertIndicatorImage(aState, iconColor).ToNSImage();
    [self setImage:tabIndicatorImage];
    affordanceImage_.reset([tabIndicatorImage retain]);
  }
}

- (void)transitionToAlertState:(TabAlertState)nextState {
  if (nextState == alertState_)
    return;

  [self updateIconForState:nextState];

  if ((alertState_ == TabAlertState::AUDIO_PLAYING &&
       nextState == TabAlertState::AUDIO_MUTING) ||
      (alertState_ == TabAlertState::AUDIO_MUTING &&
       nextState == TabAlertState::AUDIO_PLAYING) ||
      (alertState_ == TabAlertState::AUDIO_MUTING &&
       nextState == TabAlertState::NONE)) {
    // Instant user feedback: No fade animation.
    showingAlertState_ = nextState;
    fadeAnimation_.reset();
  } else {
    if (nextState == TabAlertState::NONE)
      showingAlertState_ = alertState_;  // Fading-out indicator.
    else
      showingAlertState_ = nextState;  // Fading-in to next indicator.
    // gfx::Animation requires a task runner is available for the current
    // thread.  Generally, only certain unit tests would not instantiate a task
    // runner.
    if (base::ThreadTaskRunnerHandle::IsSet()) {
      fadeAnimation_ = chrome::CreateTabAlertIndicatorFadeAnimation(nextState);
      if (!fadeAnimationDelegate_)
        fadeAnimationDelegate_.reset(new FadeAnimationDelegate(self));
      fadeAnimation_->set_delegate(fadeAnimationDelegate_.get());
      fadeAnimation_->Start();
    }
  }

  alertState_ = nextState;

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
  if (alertState_ == TabAlertState::NONE)
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

  if (alertState_ == TabAlertState::AUDIO_PLAYING)
    content::RecordAction(UserMetricsAction("AlertIndicatorButton_Mute"));
  else if (alertState_ == TabAlertState::AUDIO_MUTING)
    content::RecordAction(UserMetricsAction("AlertIndicatorButton_Unmute"));
  else
    NOTREACHED();

  [clickTarget_ performSelector:clickAction_ withObject:self];
}

- (void)updateEnabledForMuteToggle {
  BOOL enable = chrome::AreExperimentalMuteControlsEnabled() &&
      (alertState_ == TabAlertState::AUDIO_PLAYING ||
       alertState_ == TabAlertState::AUDIO_MUTING);

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

// ThemedWindowDrawing protocol support.

- (void)windowDidChangeTheme {
  // Force the alert icon to update because the icon color may change based
  // on the current theme.
  [self updateIconForState:alertState_];
}

- (void)windowDidChangeActive {
}

@end
