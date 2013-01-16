// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/location_bar/web_intents_button_decoration.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#import "chrome/browser/ui/cocoa/last_active_browser_cocoa.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#import "chrome/browser/ui/cocoa/omnibox/omnibox_view_mac.h"
#include "chrome/browser/ui/omnibox/location_bar_util.h"
#include "chrome/browser/ui/intents/web_intent_picker_controller.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"

namespace {

// Duration of animation, 150 ms.
const NSTimeInterval kAnimationDurationS = 0.15;

// Interval of the animation timer, 60Hz.
const NSTimeInterval kAnimationIntervalS = 1.0 / 60.0;

const CGFloat kTextMarginPadding = 4;
const CGFloat kIconMarginPadding = 2;
const CGFloat kBorderPadding = 3;
const CGFloat kBubbleYInset = 4.0;

// During animation, the text opens to full width.
enum AnimationState {
  kNoAnimation,
  kOpening,
  kOpen
};

}  // namespace

// An ObjC class that handles the multiple states of the text animation and
// bridges NSTimer calls back to the WebIntentsButtonDecoration that owns it.
// Should be lazily instantiated to only exist when the decoration requires
// animation.
// NOTE: One could make this class more generic, but this class only exists
// because CoreAnimation cannot be used (there are no views to work with).
@interface WebIntentsButtonAnimationState : NSObject {
 @private
  WebIntentsButtonDecoration* owner_;  // Weak, owns this.
  double progress_;  // Counter, [0..1], with animation progress.
  NSTimer* timer_;  // Animation timer. Owned by the run loop.
}

// [0..1], the current progress of the animation. -animationState will return
// |kNoAnimation| when progress is <= 0 or >= 1. Useful when state is
// |kOpening| as a multiplier for displaying width. Don't use
// to track state transitions, use -animationState instead.
@property (readonly, nonatomic) double progress;

// Designated initializer. |owner| must not be nil. Animation timer will start
// as soon as the object is created.
- (id)initWithOwner:(WebIntentsButtonDecoration*)owner;

// Returns the current animation state based on how much time has elapsed.
- (AnimationState)animationState;

// Call when |owner| is going away or the animation needs to be stopped.
// Ensures that any dangling references are cleared. Can be called multiple
// times.
- (void)stopAnimation;

@end

@implementation WebIntentsButtonAnimationState

@synthesize progress = progress_;

- (id)initWithOwner:(WebIntentsButtonDecoration*)owner {
  if ((self = [super init])) {
    owner_ = owner;
    timer_ = [NSTimer scheduledTimerWithTimeInterval:kAnimationIntervalS
                                              target:self
                                            selector:@selector(timerFired:)
                                            userInfo:nil
                                             repeats:YES];
  }
  return self;
}

- (void)dealloc {
  DCHECK(!timer_);
  [super dealloc];
}

// Clear weak references and stop the timer.
- (void)stopAnimation {
  owner_ = nil;
  [timer_ invalidate];
  timer_ = nil;
}

// Returns the current state based on how much time has elapsed.
- (AnimationState)animationState {
  if (progress_ <= 0.0)
    return kNoAnimation;
  if (progress_ <= 1.0)
    return kOpening;
  return kOpen;
}

- (void)timerFired:(NSTimer*)timer {
  // Increment animation progress, normalized to [0..1].
  // TODO(gbillock): Note this is brittle. Switch to ui/base/animation.
  progress_ += kAnimationIntervalS / kAnimationDurationS;
  progress_ = std::min(progress_, 1.0);
  owner_->AnimationTimerFired();
  // Stop timer if it has reached the end of its life.
  if (progress_ >= 1.0)
    [self stopAnimation];
}

@end

WebIntentsButtonDecoration::WebIntentsButtonDecoration(
    LocationBarViewMac* owner, NSFont* font)
    : BubbleDecoration(font),
      owner_(owner),
      text_width_(0.0) {
  NSColor* border_color =
      [NSColor colorWithCalibratedRed:0.63 green:0.63 blue:0.63 alpha:1.0];
  NSColor* background_color =
      [NSColor colorWithCalibratedRed:0.90 green:0.90 blue:0.90 alpha:1.0];
  SetColors(border_color, background_color, [NSColor blackColor]);

  SetLabel(l10n_util::GetNSString(IDS_INTENT_PICKER_USE_ANOTHER_SERVICE));
}

WebIntentsButtonDecoration::~WebIntentsButtonDecoration() {}

void WebIntentsButtonDecoration::SetButtonImages(NSImage* left,
                                                 NSImage* center,
                                                 NSImage* right) {
  left_image_.reset([left retain]);
  center_image_.reset([center retain]);
  right_image_.reset([right retain]);
}

bool WebIntentsButtonDecoration::AcceptsMousePress() {
  return true;
}

bool WebIntentsButtonDecoration::OnMousePressed(NSRect frame) {
  content::WebContents* web_contents = owner_->GetWebContents();
  if (!web_contents)
    return true;

  WebIntentPickerController::FromWebContents(web_contents)->
      LocationBarPickerButtonClicked();
  return true;
}

// Override to handle the case where there is text to display during the
// animation. The width is based on the animator's progress.
CGFloat WebIntentsButtonDecoration::GetWidthForSpace(CGFloat width) {
  if (!animation_)
    return BubbleDecoration::GetWidthForSpace(width);

  AnimationState state = [animation_ animationState];
  CGFloat progress = [animation_ progress];
  // Set the margins, fixed for all animation states.
  CGFloat preferredWidth = 2 * kTextMarginPadding;
  // Add the width of the text based on the state of the animation.
  switch (state) {
    case kNoAnimation:
      return preferredWidth;
    case kOpening:
      return preferredWidth + (text_width_ * progress);
    case kOpen:
      return preferredWidth + text_width_;
  }
}

void WebIntentsButtonDecoration::DrawInFrame(NSRect frame,
                                             NSView* control_view) {
  if ([animation_ animationState] != kOpening) {
    BubbleDecoration::DrawInFrame(frame, control_view);
    return;
  }

  frame = NSInsetRect(frame, 0.0, kBubbleYInset);
  NSDrawThreePartImage(frame,
                       left_image_.get(),
                       center_image_.get(),
                       right_image_.get(),
                       NO,  // NO=horizontal layout
                       NSCompositeSourceOver,
                       1.0,
                       YES);  // use flipped coordinates

  // Draw the text, clipped to fit on the right. While handling clipping,
  // NSAttributedString's drawInRect: won't draw a word if it doesn't fit
  // in the bounding box so instead use drawAtPoint: with a manual clip
  // rect.
  gfx::ScopedNSGraphicsContextSaveGState scopedGState;
  NSRectClip(frame);
  NSRect remainder = NSInsetRect(frame, kTextMarginPadding, 0.0);
  // .get() needed to fix compiler warning (confusion with NSImageRep).
  [animated_text_.get() drawAtPoint:remainder.origin];
}

void WebIntentsButtonDecoration::Update(content::WebContents* web_contents) {
  WebIntentPickerController* intents_controller =
      WebIntentPickerController::FromWebContents(web_contents);
  SetVisible(intents_controller->ShowLocationBarPickerButton());

  if (IsVisible()) {
    if (!animation_ &&
        !intents_controller->location_bar_picker_button_indicated()) {
      intents_controller->SetLocationBarPickerButtonIndicated();
      animation_.reset(
          [[WebIntentsButtonAnimationState alloc] initWithOwner:this]);
      animated_text_ = CreateAnimatedText();
      text_width_ = MeasureTextWidth();
    }
  } else {
    [animation_ stopAnimation];
    animation_.reset();
  }
}

scoped_nsobject<NSAttributedString>
    WebIntentsButtonDecoration::CreateAnimatedText() {
  NSString* text =
      l10n_util::GetNSString(IDS_INTENT_PICKER_USE_ANOTHER_SERVICE);
  return scoped_nsobject<NSAttributedString>(
      [[NSAttributedString alloc] initWithString:text attributes:attributes_]);
}

CGFloat WebIntentsButtonDecoration::MeasureTextWidth() {
  return [animated_text_ size].width;
}

void WebIntentsButtonDecoration::AnimationTimerFired() {
  if (owner_)
    owner_->Layout();
  // Even after the animation completes, the |animator_| object should be kept
  // alive to prevent the animation from re-appearing if the page opens
  // additional popups later. The animator will be cleared when the decoration
  // hides, indicating something has changed with the WebContents (probably
  // navigation).
}
