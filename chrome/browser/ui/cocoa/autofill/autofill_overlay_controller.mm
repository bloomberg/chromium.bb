// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_overlay_controller.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_dialog_types.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view_delegate.h"
#include "chrome/browser/ui/cocoa/autofill/autofill_dialog_constants.h"
#include "skia/ext/skia_utils_mac.h"

namespace {

// Spacing around the message in the overlay view.
const CGFloat kOverlayLabelPadding = 34;

// Spacing below image and above text messages in overlay view.
const CGFloat kOverlayImageVerticalPadding = 90;

// TODO(groby): Unify colors with Views.
// Slight shading for mouse hover and legal document background.
SkColor kShadingColor = 0xfff2f2f2;

// A border color for the legal document view.
SkColor kSubtleBorderColor = 0xffdfdfdf;

}  // namespace

// An NSView encapsulating the message stack and its custom drawn elements.
@interface AutofillMessageView : NSView<AutofillLayout> {
 @private
  base::scoped_nsobject<NSTextField> label_;
}

- (id)initWithFrame:(NSRect)frame;
- (CGFloat)heightForWidth:(CGFloat)width;
- (void)setMessage:(const autofill::DialogOverlayString&)message;
@end


@implementation AutofillMessageView

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

- (id)initWithFrame:(NSRect)frame {
  if (self = [super initWithFrame:frame]) {
    label_.reset([[NSTextField alloc] initWithFrame:NSZeroRect]);
    [label_ setEditable:NO];
    [label_ setBordered:NO];
    [label_ setDrawsBackground:NO];
    [label_ setAlignment:NSCenterTextAlignment];
    [self addSubview:label_];
  }
  return self;
}

- (CGFloat)heightForWidth:(CGFloat)width {
  return NSHeight([label_ frame]) + autofill::kArrowHeight +
      2 * kOverlayLabelPadding;
}

- (void)setMessage:(const autofill::DialogOverlayString&)message {
  // We probably want to look at other multi-line messages somewhere.
  [label_ setFont:message.font_list.GetPrimaryFont().GetNativeFont()];
  [label_ setStringValue:base::SysUTF16ToNSString(message.text)];
  [label_ setTextColor:[NSColor darkGrayColor]];

  // Resize only height, preserve width. This guarantees text stays centered in
  // the dialog.
  NSSize labelSize = [label_ frame].size;
  labelSize.height = [[label_ cell] cellSize].height;
  [label_ setFrameSize:labelSize];

  [self setHidden:message.text.empty()];
}

- (void)performLayout {
  if ([self isHidden])
    return;

  CGFloat labelHeight = NSHeight([label_ frame]);
  [label_ setFrame:NSMakeRect(0, kOverlayLabelPadding,
                              NSWidth([self bounds]), labelHeight)];
  // TODO(groby): useful DCHECK() goes here.
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

    messageView_.reset([[AutofillMessageView alloc] initWithFrame:NSZeroRect]);
    imageView_.reset([[NSImageView alloc] initWithFrame:NSZeroRect]);
    [imageView_ setImageAlignment:NSImageAlignCenter];

    base::scoped_nsobject<NSView> view(
        [[NSView alloc] initWithFrame:NSZeroRect]);
    [view setSubviews:@[messageView_, imageView_]];
    [self setView:view];
  }
  return self;
}

- (void)updateState {
  const autofill::DialogOverlayState& state = delegate_->GetDialogOverlay();

  if (state.image.IsEmpty()) {
    [[self view] setHidden:YES];
    return;
  }

  [[self view] setHidden:NO];
  [imageView_ setImage:state.image.ToNSImage()];
  [messageView_ setMessage:state.string];

  NSWindowController* delegate = [[[self view] window] windowController];
  if ([delegate respondsToSelector:@selector(requestRelayout)])
    [delegate performSelector:@selector(requestRelayout)];
}

- (CGFloat)heightForWidth:(int)width {
  return 2 * kOverlayImageVerticalPadding +
      [messageView_ heightForWidth:width] +
      [[imageView_ image] size].height;
}

- (NSSize)preferredSize {
  NOTREACHED();  // Only implemented as part of AutofillLayout protocol.
  return NSZeroSize;
}

- (void)performLayout {
  NSRect bounds = [[self view] bounds];

  int messageHeight = [messageView_ heightForWidth:NSWidth(bounds)];
  [messageView_ setFrame:NSMakeRect(0, 0, NSWidth(bounds), messageHeight)];
  [messageView_ performLayout];

  NSSize imageSize = [[imageView_ image] size];
  [imageView_ setFrame:NSMakeRect(
       0, NSMaxY([messageView_ frame]) + kOverlayImageVerticalPadding,
       NSWidth(bounds), imageSize.height)];
}

@end
