// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_loading_shield_controller.h"

#include <cmath>

#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view_delegate.h"
#include "chrome/browser/ui/autofill/loading_animation.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/font_list.h"

namespace {

// Horizontal spacing between the animated dots.
// Using a negative spacing creates an ellipsis-like effect.
// TODO(isherman): Consider using the recipe below instead:
//   Create NSBezierPath
//   -[NSBezierPath appendBezierPathWithGlyph:inFont:]
//   -[NSBezierPath bounds]
const CGFloat kDotsHorizontalPadding = -6;

}  // namespace


// A C++ bridge class for driving the animation.
class AutofillLoadingAnimationBridge : public gfx::AnimationDelegate {
 public:
  AutofillLoadingAnimationBridge(AutofillLoadingShieldController* controller,
                                 int font_size)
      : animation_(this, font_size),
        controller_(controller) {}
  virtual ~AutofillLoadingAnimationBridge() {}

  // gfx::AnimationDelegate implementation.
  virtual void AnimationProgressed(const gfx::Animation* animation) OVERRIDE {
    DCHECK_EQ(animation, &animation_);
    [controller_ relayoutDotsForSteppedAnimation:animation_];
  }

  autofill::LoadingAnimation* animation() { return &animation_; }

 private:
  autofill::LoadingAnimation animation_;
  AutofillLoadingShieldController* const controller_;  // weak, owns |this|
};


@implementation AutofillLoadingShieldController

- (id)initWithDelegate:(autofill::AutofillDialogViewDelegate*)delegate {
  if (self = [super initWithNibName:nil bundle:nil]) {
    delegate_ = delegate;

    const gfx::FontList& font_list =
        ui::ResourceBundle::GetSharedInstance().GetFontList(
            ui::ResourceBundle::LargeFont);
    NSFont* native_font = font_list.GetPrimaryFont().GetNativeFont();
    animationDriver_.reset(
        new AutofillLoadingAnimationBridge(self, font_list.GetHeight()));

    base::scoped_nsobject<NSBox> view([[NSBox alloc] initWithFrame:NSZeroRect]);
    [view setBoxType:NSBoxCustom];
    [view setBorderType:NSNoBorder];
    [view setContentViewMargins:NSZeroSize];
    [view setTitlePosition:NSNoTitle];

    message_.reset([[NSTextField alloc] initWithFrame:NSZeroRect]);
    [message_ setFont:native_font];
    [message_ setEditable:NO];
    [message_ setBordered:NO];
    [message_ setDrawsBackground:NO];
    [view addSubview:message_];

    dots_.reset([[NSArray alloc] initWithArray:@[
         [[NSTextField alloc] initWithFrame:NSZeroRect],
         [[NSTextField alloc] initWithFrame:NSZeroRect],
         [[NSTextField alloc] initWithFrame:NSZeroRect] ]]);
    NSInteger tag = 0;
    for (NSTextField* dot in dots_.get()) {
      [dot setFont:native_font];
      [dot setEditable:NO];
      [dot setBordered:NO];
      [dot setDrawsBackground:NO];
      [dot setStringValue:@"."];
      [dot setTag:tag];
      [dot sizeToFit];
      [view addSubview:dot];

      ++tag;
    }

    [self setView:view];
  }
  return self;
}

- (void)update {
  NSString* newMessage = @"";
  if (delegate_->ShouldShowSpinner())
    newMessage = base::SysUTF16ToNSString(delegate_->SpinnerText());

  if ([newMessage isEqualToString:[message_ stringValue]])
    return;

  [message_ setStringValue:newMessage];
  [message_ sizeToFit];

  if ([newMessage length] > 0) {
    [[self view] setHidden:NO];
    animationDriver_->animation()->Start();
  } else {
    [[self view] setHidden:YES];
    animationDriver_->animation()->Reset();
  }

  NSWindowController* delegate = [[[self view] window] windowController];
  if ([delegate respondsToSelector:@selector(requestRelayout)])
    [delegate performSelector:@selector(requestRelayout)];
}

- (NSSize)preferredSize {
  NOTREACHED();  // Only implemented as part of AutofillLayout protocol.
  return NSZeroSize;
}

- (void)performLayout {
  if ([[self view] isHidden])
    return;

  NSRect bounds = [[self view] bounds];
  NSRect messageFrame = [message_ frame];

  NSSize size = messageFrame.size;
  for (NSView* dot in dots_.get()) {
    size.width += NSWidth([dot frame]) + kDotsHorizontalPadding;
    size.height = std::max(size.height, NSHeight([dot frame]));
  }

  // The message + dots should be centered in the view.
  messageFrame.origin.x = std::ceil((NSWidth(bounds) - size.width) / 2.0);
  messageFrame.origin.y = std::ceil((NSHeight(bounds) - size.height) / 2.0);
  [message_ setFrameOrigin:messageFrame.origin];

  NSView* previousView = message_;
  for (NSView* dot in dots_.get()) {
    NSPoint dotFrameOrigin =
        NSMakePoint(NSMaxX([previousView frame]) + kDotsHorizontalPadding,
                    messageFrame.origin.y);
    [dot setFrameOrigin:dotFrameOrigin];

    previousView = dot;
  }
}

- (void)relayoutDotsForSteppedAnimation:
    (const autofill::LoadingAnimation&)animation {
  for (NSView* dot in dots_.get()) {
    NSPoint origin = [dot frame].origin;
    origin.y = [message_ frame].origin.y -
        animation.GetCurrentValueForDot([dot tag]);
    [dot setFrameOrigin:origin];
  }
}

@end
