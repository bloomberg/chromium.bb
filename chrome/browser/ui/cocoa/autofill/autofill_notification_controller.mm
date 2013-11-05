// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_notification_controller.h"

#include <algorithm>

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_dialog_types.h"
#include "chrome/browser/ui/chrome_style.h"
#include "chrome/browser/ui/cocoa/autofill/autofill_dialog_constants.h"
#include "grit/theme_resources.h"
#include "skia/ext/skia_utils_mac.h"

@interface AutofillNotificationView : NSView {
 @private
  // Weak, determines anchor point for arrow.
  NSView* arrowAnchorView_;
  BOOL hasArrow_;
  base::scoped_nsobject<NSColor> backgroundColor_;
  base::scoped_nsobject<NSColor> borderColor_;
}

@property (nonatomic, assign) NSView* anchorView;
@property (nonatomic, assign) BOOL hasArrow;
@property (nonatomic, retain) NSColor* backgroundColor;
@property (nonatomic, retain) NSColor* borderColor;

@end

@implementation AutofillNotificationView

@synthesize hasArrow = hasArrow_;
@synthesize anchorView = arrowAnchorView_;

- (void)drawRect:(NSRect)dirtyRect {
  [super drawRect:dirtyRect];

  NSBezierPath* path;
  NSRect bounds = [self bounds];
  if (!hasArrow_) {
    path = [NSBezierPath bezierPathWithRect:bounds];
  } else {
    // The upper tip of the arrow.
    NSPoint anchorPoint = NSMakePoint(NSMidX([arrowAnchorView_ bounds]), 0);
    anchorPoint = [self convertPoint:anchorPoint fromView:arrowAnchorView_];
    anchorPoint.y = NSMaxY(bounds);
    // The minimal rectangle that encloses the arrow.
    NSRect arrowRect = NSMakeRect(anchorPoint.x - autofill::kArrowWidth / 2.0,
                                  anchorPoint.y - autofill::kArrowHeight,
                                  autofill::kArrowWidth,
                                  autofill::kArrowHeight);

    // Include the arrow and the rectangular non-arrow region in the same path,
    // so that the stroke is easier to draw. Start at the upper-left of the
    // rectangular region, and proceed clockwise.
    path = [NSBezierPath bezierPath];
    [path moveToPoint:NSMakePoint(NSMinX(bounds), NSMinY(arrowRect))];
    [path lineToPoint:arrowRect.origin];
    [path lineToPoint:NSMakePoint(NSMidX(arrowRect), NSMaxY(arrowRect))];
    [path lineToPoint:NSMakePoint(NSMaxX(arrowRect), NSMinY(arrowRect))];
    [path lineToPoint:NSMakePoint(NSMaxX(bounds), NSMinY(arrowRect))];
    [path lineToPoint:NSMakePoint(NSMaxX(bounds), NSMinY(bounds))];
    [path lineToPoint:NSMakePoint(NSMinX(bounds), NSMinY(bounds))];
    [path closePath];
  }

  [backgroundColor_ setFill];
  [path fill];
  [borderColor_ setStroke];
  [path stroke];
}

- (NSColor*)backgroundColor {
  return backgroundColor_;
}

- (void)setBackgroundColor:(NSColor*)backgroundColor {
  backgroundColor_.reset([backgroundColor retain]);
}

- (NSColor*)borderColor {
  return borderColor_;
}

- (void)setBorderColor:(NSColor*)borderColor {
  borderColor_.reset([borderColor retain]);
}

@end

@implementation AutofillNotificationController

- (id)initWithNotification:(const autofill::DialogNotification*)notification {
  if (self = [super init]) {
    base::scoped_nsobject<AutofillNotificationView> view(
        [[AutofillNotificationView alloc] initWithFrame:NSZeroRect]);
    [view setBackgroundColor:
        gfx::SkColorToCalibratedNSColor(notification->GetBackgroundColor())];
    [view setBorderColor:
        gfx::SkColorToCalibratedNSColor(notification->GetBorderColor())];
    [self setView:view];

    textfield_.reset([[NSTextField alloc] initWithFrame:NSZeroRect]);
    [textfield_ setEditable:NO];
    [textfield_ setBordered:NO];
    [textfield_ setDrawsBackground:NO];
    [textfield_ setTextColor:
         gfx::SkColorToCalibratedNSColor(notification->GetTextColor())];
    [textfield_ setStringValue:
        base::SysUTF16ToNSString(notification->display_text())];
    [textfield_ setHidden:notification->HasCheckbox()];

    checkbox_.reset([[NSButton alloc] initWithFrame:NSZeroRect]);
    [checkbox_ setButtonType:NSSwitchButton];
    [checkbox_ setHidden:!notification->HasCheckbox()];
    [checkbox_ setState:(notification->checked() ? NSOnState : NSOffState)];
    [checkbox_ setAttributedTitle:[textfield_ attributedStringValue]];
    // Update the size that preferredSizeForWidth will use. Do this here because
    //   (1) preferredSizeForWidth is logically const, and so shouldn't have a
    //       side-effect of updating the checkbox's frame, and
    //   (2) this way, the sizing computation can be cached.
    [checkbox_ sizeToFit];

    tooltipIcon_.reset([[NSImageView alloc] initWithFrame:NSZeroRect]);
    [tooltipIcon_ setImage:
        ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
            IDR_AUTOFILL_TOOLTIP_ICON).ToNSImage()];
    [tooltipIcon_ setFrameSize:[[tooltipIcon_ image] size]];
    [tooltipIcon_ setToolTip:
        base::SysUTF16ToNSString(notification->tooltip_text())];
    [tooltipIcon_ setHidden:[[tooltipIcon_ toolTip] length] == 0];

    [view setSubviews:@[textfield_, checkbox_, tooltipIcon_]];
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

- (NSTextField*)textfield {
  return textfield_;
}

- (NSButton*)checkbox {
  return checkbox_;
}

- (NSImageView*)tooltipIcon {
  return tooltipIcon_;
}

- (NSSize)preferredSizeForWidth:(CGFloat)width {
  width -= 2 * chrome_style::kHorizontalPadding;
  if (![tooltipIcon_ isHidden])
    width -= [tooltipIcon_ frame].size.width + chrome_style::kHorizontalPadding;
  // TODO(isherman): Restore the DCHECK below once I figure out why it causes
  // unit tests to fail.
  //DCHECK_GT(width, 0);

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

  // Calculate the frame size, leaving room for padding around the notification,
  // as well as for the tooltip if it is visible.
  NSRect labelFrame = NSInsetRect(bounds,
                                 chrome_style::kHorizontalPadding,
                                 autofill::kNotificationPadding);
  if (![tooltipIcon_ isHidden]) {
    labelFrame.size.width -=
        [tooltipIcon_ frame].size.width + chrome_style::kHorizontalPadding;
  }

  NSView* label = [checkbox_ isHidden] ? textfield_.get() : checkbox_.get();
  [label setFrame:labelFrame];

  if (![tooltipIcon_ isHidden]) {
    NSPoint tooltipOrigin =
        NSMakePoint(
            NSMaxX(labelFrame) + chrome_style::kHorizontalPadding,
            NSMidY(labelFrame) - (NSHeight([tooltipIcon_ frame]) / 2.0));
    [tooltipIcon_ setFrameOrigin:tooltipOrigin];
  }
}

@end
