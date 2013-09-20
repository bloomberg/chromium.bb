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

// Spacing between lines of text in the overlay view.
const CGFloat kOverlayTextInterlineSpacing = 10;

// Spacing below image and above text messages in overlay view.
const CGFloat kOverlayImageBottomMargin = 50;

// TODO(groby): Unify colors with Views.
// Slight shading for mouse hover and legal document background.
SkColor kShadingColor = 0xfff2f2f2;

// A border color for the legal document view.
SkColor kSubtleBorderColor = 0xffdfdfdf;

}  // namespace

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
  for (NSTextField* label in [self subviews]) {
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
  for (NSTextField* label in [self subviews]) {
    DCHECK([label isKindOfClass:[NSTextField class]]);
    CGFloat labelHeight = NSHeight([label frame]);
    [label setFrame:NSMakeRect(0, y - labelHeight,
                               NSWidth([self bounds]), labelHeight)];
    y = NSMinY([label frame]) - kOverlayTextInterlineSpacing;
  }
  DCHECK_GE(y, 0.0);
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
  const autofill::DialogOverlayState& state = delegate_->GetDialogOverlay();

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

@end
