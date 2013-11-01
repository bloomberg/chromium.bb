// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_notification_controller.h"

#include <algorithm>

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/ui/chrome_style.h"
#include "chrome/browser/ui/cocoa/autofill/autofill_dialog_constants.h"

@interface AutofillNotificationView : NSView {
 @private
  // Weak, determines anchor point for arrow.
  NSView* arrowAnchorView_;
  BOOL hasArrow_;
  base::scoped_nsobject<NSColor> backgroundColor_;
}

@property (nonatomic, assign) NSView* anchorView;
@property (nonatomic, assign) BOOL hasArrow;
@property (nonatomic, retain) NSColor* backgroundColor;

@end

@implementation AutofillNotificationView

@synthesize hasArrow = hasArrow_;
@synthesize anchorView = arrowAnchorView_;

- (void)drawRect:(NSRect)dirtyRect {
  [super drawRect:dirtyRect];

  NSRect backgroundRect = [self bounds];
  if (hasArrow_) {
    NSPoint anchorPoint = NSMakePoint(NSMidX([arrowAnchorView_ bounds]), 0);
    anchorPoint = [self convertPoint:anchorPoint fromView:arrowAnchorView_];
    anchorPoint.y = NSMaxY([self bounds]);

    NSBezierPath* arrow = [NSBezierPath bezierPath];
    [arrow moveToPoint:anchorPoint];
    [arrow relativeLineToPoint:
        NSMakePoint(-autofill::kArrowWidth / 2.0, -autofill::kArrowHeight)];
    [arrow relativeLineToPoint:NSMakePoint(autofill::kArrowWidth, 0)];
    [arrow closePath];
    [backgroundColor_ setFill];
    [arrow fill];
    backgroundRect.size.height -= autofill::kArrowHeight;
  }

  dirtyRect = NSIntersectionRect(backgroundRect, dirtyRect);
  [backgroundColor_ setFill];
  NSRectFill(dirtyRect);
}

- (NSColor*)backgroundColor {
  return backgroundColor_;
}

- (void)setBackgroundColor:(NSColor*)backgroundColor {
  backgroundColor_.reset([backgroundColor retain]);
}

@end

@implementation AutofillNotificationController

- (id)init {
  if (self = [super init]) {
    base::scoped_nsobject<AutofillNotificationView> view(
        [[AutofillNotificationView alloc] initWithFrame:NSZeroRect]);
    [self setView:view];

    textfield_.reset([[NSTextField alloc] initWithFrame:NSZeroRect]);
    [textfield_ setEditable:NO];
    [textfield_ setBordered:NO];
    [textfield_ setDrawsBackground:NO];

    checkbox_.reset([[NSButton alloc] initWithFrame:NSZeroRect]);
    [checkbox_ setButtonType:NSSwitchButton];
    [checkbox_ setHidden:YES];
    [view setSubviews:@[textfield_, checkbox_]];
  }
  return self;
}

- (AutofillNotificationView*)notificationView {
  return base::mac::ObjCCastStrict<AutofillNotificationView>([self view]);
}

- (void)setHasArrow:(BOOL)hasArrow withAnchorView:(NSView*)anchorView {
  [[self notificationView] setAnchorView:anchorView];
  [[self notificationView] setHasArrow:hasArrow];
}

- (BOOL)hasArrow {
  return [[self notificationView] hasArrow];
}

- (void)setHasCheckbox:(BOOL)hasCheckbox {
  [checkbox_ setHidden:!hasCheckbox];
  [textfield_ setHidden:hasCheckbox];
}

- (NSString*)text {
  return [textfield_ stringValue];
}

- (void)setText:(NSString*)string {
  [textfield_ setStringValue:string];
  [checkbox_ setAttributedTitle:[textfield_ attributedStringValue]];

  // Update the size that preferredSizeForWidth will use. Do this here because
  //   (1) preferredSizeForWidth is logically const, and so shouldn't have a
  //       side-effect of updating the checkbox's frame, and
  //   (2) this way, the sizing computation can be cached.
  [checkbox_ sizeToFit];
}

- (NSTextField*)textfield {
  return textfield_;
}

- (NSButton*)checkbox {
  return checkbox_;
}

- (NSColor*)backgroundColor {
  return [[self notificationView] backgroundColor];
}

- (void)setBackgroundColor:(NSColor*)backgroundColor {
  [[self notificationView] setBackgroundColor:backgroundColor];
}

- (NSColor*)textColor {
  return [textfield_ textColor];
}

- (void)setTextColor:(NSColor*)textColor {
  [textfield_ setTextColor:textColor];
}

- (NSSize)preferredSizeForWidth:(CGFloat)width {
  NSSize preferredSize;
  if (![textfield_ isHidden]) {
    NSRect bounds = NSMakeRect(0, 0, width, CGFLOAT_MAX);
    preferredSize = [[textfield_ cell] cellSizeForBounds:bounds];
  } else {
    // Unlike textfields, checkboxes (NSButtons, really) are not designed to
    // support multi-line labels. Hence, ignore the |width| and simply use the
    // size that fits fit the checkbox's contents.
    // NOTE: This logic will need to be updated if there is ever a need to
    // support checkboxes with multi-line labels.
    DCHECK(![checkbox_ isHidden]);
    preferredSize = [checkbox_ frame].size;
  }

  if ([[self notificationView] hasArrow])
      preferredSize.height += autofill::kArrowHeight;

  preferredSize.height += 2 * autofill::kNotificationPadding;
  return preferredSize;
}

- (NSSize)preferredSize {
  NOTREACHED();
  return NSZeroSize;
}

- (void)performLayout {
  NSRect bounds = [[self view] bounds];
  if ([[self notificationView] hasArrow])
    bounds.size.height -= autofill::kArrowHeight;

  NSRect textFrame = NSInsetRect(bounds,
                                 chrome_style::kHorizontalPadding,
                                 autofill::kNotificationPadding);
  NSControl* control =
      [checkbox_ isHidden] ? textfield_.get() : checkbox_.get();
  [control setFrame:textFrame];
}

@end
