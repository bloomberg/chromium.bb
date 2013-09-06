// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_overlay_controller.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/autofill/autofill_dialog_types.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view_delegate.h"
#include "chrome/browser/ui/cocoa/autofill/autofill_dialog_constants.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/animation/multi_animation.h"

namespace {

// Spacing between lines of text in the overlay view.
const CGFloat kOverlayTextInterlineSpacing = 10;

// Spacing below image and above text messages in overlay view.
const CGFloat kOverlayImageBottomMargin = 50;

// TODO(groby): Unify colors with Views.
// Slight shading for mouse hover and legal document background.
SkColor kShadingColor = 0xfff2f2f2;

// A border color for the legal document view.
SkColor kSubtleBorderColor = 0xffdfdfdf;

// Shorten a few long types.
typedef ui::MultiAnimation::Part Part;
typedef ui::MultiAnimation::Parts Parts;

}  // namespace

// Bridges Objective C and C++ delegate interfaces.
class AnimationDelegateBridge : public ui::AnimationDelegate {
 public:
  AnimationDelegateBridge(id<AnimationDelegate> delegate);

 protected:
  // AnimationDelegate implementation.
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;
  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE;

 private:
  id<AnimationDelegate> delegate_;  // Not owned. Owns DelegateBridge.
  DISALLOW_COPY_AND_ASSIGN(AnimationDelegateBridge);
};

AnimationDelegateBridge::AnimationDelegateBridge(
    id<AnimationDelegate> delegate) : delegate_(delegate) {}

void AnimationDelegateBridge::AnimationProgressed(
    const ui::Animation* animation) {
  [delegate_ animationProgressed:animation];
}

void AnimationDelegateBridge::AnimationEnded(
    const ui::Animation* animation) {
  [delegate_ animationEnded:animation];
}

class OverlayTimerBridge {
 public:
  OverlayTimerBridge(AutofillOverlayController* controller);
  void SetExpiry(const base::TimeDelta& delta);

 private:
  void UpdateOverlayState();

  // Controls when the overlay should request a status update.
  base::OneShotTimer<OverlayTimerBridge> refresh_timer_;

  // not owned, |overlay_view_controller_| owns |this|.
  AutofillOverlayController* overlay_view_controller_;

  DISALLOW_COPY_AND_ASSIGN(OverlayTimerBridge);
};

OverlayTimerBridge::OverlayTimerBridge(AutofillOverlayController* controller)
    : overlay_view_controller_(controller) {
}

void OverlayTimerBridge::SetExpiry(const base::TimeDelta& expiry) {
  if (expiry != base::TimeDelta()) {
    refresh_timer_.Start(FROM_HERE,
                         expiry,
                         this,
                         &OverlayTimerBridge::UpdateOverlayState);
  } else {
    refresh_timer_.Stop();
  }
}

void OverlayTimerBridge::UpdateOverlayState() {
  [overlay_view_controller_ updateState];
}

// An NSView encapsulating the message stack and its custom drawn elements.
@interface AutofillMessageStackView : NSView<AutofillLayout>
- (CGFloat)heightForWidth:(CGFloat)width;
- (void)setMessages:
      (const std::vector<autofill::DialogOverlayString>&)messages;
@end


@implementation AutofillMessageStackView

- (void)drawRect:(NSRect)dirtyRect {
  NSColor* shadingColor = gfx::SkColorToCalibratedNSColor(kShadingColor);
  NSColor* borderColor = gfx::SkColorToCalibratedNSColor(kSubtleBorderColor);

  CGFloat arrowHalfWidth = autofill::kArrowWidth / 2.0;
  NSRect bounds = [self bounds];
  CGFloat y = NSMaxY(bounds) - autofill::kArrowHeight;

  NSBezierPath* arrow = [NSBezierPath bezierPath];
  // Note that we purposely draw slightly outside of |bounds| so that the
  // stroke is hidden on the sides and bottom.
  NSRect arrowBounds = NSInsetRect(bounds, -1, -1);
  arrowBounds.size.height--;
  [arrow moveToPoint:NSMakePoint(NSMinX(arrowBounds), y)];
  [arrow lineToPoint:
      NSMakePoint(NSMidX(arrowBounds) - arrowHalfWidth, y)];
  [arrow relativeLineToPoint:
      NSMakePoint(arrowHalfWidth,autofill::kArrowHeight)];
  [arrow relativeLineToPoint:
      NSMakePoint(arrowHalfWidth, -autofill::kArrowHeight)];
  [arrow lineToPoint:NSMakePoint(NSMaxX(arrowBounds), y)];
  [arrow lineToPoint:NSMakePoint(NSMaxX(arrowBounds), NSMinY(arrowBounds))];
  [arrow lineToPoint:NSMakePoint(NSMinX(arrowBounds), NSMinY(arrowBounds))];
  [arrow closePath];

  [shadingColor setFill];
  [arrow fill];
  [borderColor setStroke];
  [arrow stroke];
}

- (CGFloat)heightForWidth:(CGFloat)width {
  CGFloat height = kOverlayTextInterlineSpacing;
  for (NSTextView* label in [self subviews]) {
    height += NSHeight([label frame]);
    height += kOverlayTextInterlineSpacing;
  }
  return height + autofill::kArrowHeight;
}

- (void)setMessages:
      (const std::vector<autofill::DialogOverlayString>&) messages {
  // We probably want to look at other multi-line messages somewhere.
  base::scoped_nsobject<NSMutableArray> labels(
      [[NSMutableArray alloc] initWithCapacity:messages.size()]);
  for (size_t i = 0; i < messages.size(); ++i) {
    base::scoped_nsobject<NSTextField> label(
        [[NSTextField alloc] initWithFrame:NSZeroRect]);

    NSFont* labelFont = messages[i].font.GetNativeFont();
    [label setEditable:NO];
    [label setBordered:NO];
    [label setDrawsBackground:NO];
    [label setFont:labelFont];
    [label setStringValue:base::SysUTF16ToNSString(messages[i].text)];
    [label setTextColor:gfx::SkColorToDeviceNSColor(messages[i].text_color)];
    DCHECK_EQ(messages[i].alignment, gfx::ALIGN_CENTER);
    [label setAlignment:NSCenterTextAlignment];
    [label sizeToFit];

    [labels addObject:label];
  }
  [self setSubviews:labels];
  [self setHidden:([labels count] == 0)];
}

- (void)performLayout {
  CGFloat y = NSMaxY([self bounds]) - autofill::kArrowHeight -
      kOverlayTextInterlineSpacing;
  for (NSTextView* label in [self subviews]) {
    DCHECK([label isKindOfClass:[NSTextView class]]);
    CGFloat labelHeight = NSHeight([label frame]);
    [label setFrame:NSMakeRect(0, y - labelHeight,
                               NSWidth([self bounds]), labelHeight)];
    y = NSMinY([label frame]) - kOverlayTextInterlineSpacing;
  }
  DCHECK_GT(0.0, y);
}

- (NSSize)preferredSize {
  NOTREACHED();
  return NSZeroSize;
}

@end


@implementation AutofillOverlayController

- (id)initWithDelegate:(autofill::AutofillDialogViewDelegate*)delegate {
  if (self = [super initWithNibName:nil bundle:nil]) {
    delegate_ = delegate;
    refreshTimer_.reset(new OverlayTimerBridge(self));

    base::scoped_nsobject<NSBox> view(
        [[NSBox alloc] initWithFrame:NSZeroRect]);
    [view setBoxType:NSBoxCustom];
    [view setBorderType:NSNoBorder];
    [view setContentViewMargins:NSZeroSize];
    [view setTitlePosition:NSNoTitle];

    childView_.reset([[NSView alloc] initWithFrame:NSZeroRect]);
    messageStackView_.reset(
        [[AutofillMessageStackView alloc] initWithFrame:NSZeroRect]);
    imageView_.reset([[NSImageView alloc] initWithFrame:NSZeroRect]);
    [imageView_ setImageAlignment:NSImageAlignCenter];

    [childView_ setSubviews:@[messageStackView_, imageView_]];
    [view addSubview:childView_];
    [self setView:view];
  }
  return self;
}

- (void)updateState {
  [self setState:delegate_->GetDialogOverlay()];
}

- (void)setState:(const autofill::DialogOverlayState&)state {
  // Don't update anything if we're still fading out the old state.
  if (fadeOutAnimation_)
    return;

  if (state.image.IsEmpty()) {
    [[self view] setHidden:YES];
    return;
  }

  NSBox* view = base::mac::ObjCCastStrict<NSBox>([self view]);
  [view setFillColor:[[view window] backgroundColor]];
  [view setAlphaValue:1];
  [childView_ setAlphaValue:1];
  [imageView_ setImage:state.image.ToNSImage()];
  [messageStackView_ setMessages:state.strings];
  [childView_ setHidden:NO];
  [view setHidden:NO];

  NSWindowController* delegate = [[[self view] window] windowController];
  if ([delegate respondsToSelector:@selector(requestRelayout)])
    [delegate performSelector:@selector(requestRelayout)];

  refreshTimer_->SetExpiry(state.expiry);
}

- (void)beginFadeOut {
  // Remove first responders, since focus rings show on top of overlay view.
  // TODO(groby): Figure out to do that less hacky. Ideally, the focus ring
  // should be part of the controls fading in, not appear at the end.
  [[self view] setNextResponder:[[[self view] window] firstResponder]];
  [[[self view] window] makeFirstResponder:[self view]];

  Parts parts;
  // For this part of the animation, simply show the splash image.
  parts.push_back(Part(autofill::kSplashDisplayDurationMs, ui::Tween::ZERO));
  // For this part of the animation, fade out the splash image.
  parts.push_back(
      Part(autofill::kSplashFadeOutDurationMs, ui::Tween::EASE_IN));
  // For this part of the animation, fade out |this| (fade in the dialog).
  parts.push_back(
      Part(autofill::kSplashFadeInDialogDurationMs, ui::Tween::EASE_OUT));
  fadeOutAnimation_.reset(
      new ui::MultiAnimation(parts,
                             ui::MultiAnimation::GetDefaultTimerInterval()));
  animationDelegate_.reset(new AnimationDelegateBridge(self));
  fadeOutAnimation_->set_delegate(animationDelegate_.get());
  fadeOutAnimation_->set_continuous(false);
  fadeOutAnimation_->Start();
}

- (CGFloat)heightForWidth:(int) width {
  // 0 means "no preference". Image-only overlays fit the container.
  if ([messageStackView_ isHidden])
    return 0;

  // Overlays with text messages express a size preference.
  return kOverlayImageBottomMargin +
      [messageStackView_ heightForWidth:width] +
      NSHeight([imageView_ frame]);
}

- (NSSize)preferredSize {
  NOTREACHED();  // Only implemented as part of AutofillLayout protocol.
  return NSZeroSize;
}

- (void)performLayout {
  NSRect bounds = [[self view] bounds];
  [childView_ setFrame:bounds];
  if ([messageStackView_ isHidden]) {
    [imageView_ setFrame:bounds];
    return;
  }

  int messageHeight = [messageStackView_ heightForWidth:NSWidth(bounds)];
  [messageStackView_ setFrame:
      NSMakeRect(0, 0, NSWidth(bounds), messageHeight)];
  [messageStackView_ performLayout];

  NSSize imageSize = [[imageView_ image] size];
  [imageView_ setFrame:NSMakeRect(
       0, NSMaxY([messageStackView_ frame]) + kOverlayImageBottomMargin,
       NSWidth(bounds), imageSize.height)];
}

- (void)animationProgressed:(const ui::Animation*)animation {
  DCHECK_EQ(animation, fadeOutAnimation_.get());

  // Fade out children in stage 1.
  if (fadeOutAnimation_->current_part_index() == 1) {
    [childView_ setAlphaValue:(1 - fadeOutAnimation_->GetCurrentValue())];
  }

  // Fade out background in stage 2(i.e. fade in what's behind |this|).
  if (fadeOutAnimation_->current_part_index() == 2) {
    [[self view] setAlphaValue: (1 - fadeOutAnimation_->GetCurrentValue())];
    [childView_ setHidden:YES];
  }

  // If any fading was done, refresh display.
  if (fadeOutAnimation_->current_part_index() != 0) {
    [[self view] setNeedsDisplay:YES];
  }
}

- (void)animationEnded:(const ui::Animation*)animation {
  DCHECK_EQ(animation, fadeOutAnimation_.get());
  [[self view] setHidden:YES];
  fadeOutAnimation_.reset();
}

@end