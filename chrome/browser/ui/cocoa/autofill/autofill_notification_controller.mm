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
#include "chrome/browser/ui/autofill/autofill_dialog_view_delegate.h"
#include "chrome/browser/ui/chrome_style.h"
#include "chrome/browser/ui/cocoa/autofill/autofill_dialog_constants.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_tooltip_controller.h"
#include "grit/theme_resources.h"
#include "skia/ext/skia_utils_mac.h"
#import "ui/base/cocoa/controls/hyperlink_text_view.h"

@interface AutofillNotificationView : NSView {
 @private
  base::scoped_nsobject<NSColor> backgroundColor_;
  base::scoped_nsobject<NSColor> borderColor_;
}

@property (nonatomic, retain) NSColor* backgroundColor;
@property (nonatomic, retain) NSColor* borderColor;

@end

@implementation AutofillNotificationView

- (void)drawRect:(NSRect)dirtyRect {
  [super drawRect:dirtyRect];

  NSBezierPath* path;
  NSRect bounds = [self bounds];
  path = [NSBezierPath bezierPathWithRect:bounds];

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

- (id)initWithNotification:(const autofill::DialogNotification*)notification
                  delegate:(autofill::AutofillDialogViewDelegate*)delegate {
  if (self = [super init]) {
    delegate_ = delegate;
    notificationType_ = notification->type();

    base::scoped_nsobject<AutofillNotificationView> view(
        [[AutofillNotificationView alloc] initWithFrame:NSZeroRect]);
    [view setBackgroundColor:
        gfx::SkColorToCalibratedNSColor(notification->GetBackgroundColor())];
    [view setBorderColor:
        gfx::SkColorToCalibratedNSColor(notification->GetBorderColor())];
    [self setView:view];

    textview_.reset([[HyperlinkTextView alloc] initWithFrame:NSZeroRect]);
    NSColor* textColor =
        gfx::SkColorToCalibratedNSColor(notification->GetTextColor());
    [textview_ setMessage:base::SysUTF16ToNSString(notification->display_text())
                 withFont:[NSFont labelFontOfSize:[[textview_ font] pointSize]]
             messageColor:textColor];
    if (!notification->link_range().is_empty()) {
      linkURL_ = notification->link_url();
      [textview_ setDelegate:self];
      [textview_ addLinkRange:notification->link_range().ToNSRange()
                      withURL:base::SysUTF8ToNSString(linkURL_.spec())
                    linkColor:[NSColor blueColor]];
    }

    tooltipController_.reset([[AutofillTooltipController alloc]
                                 initWithArrowLocation:info_bubble::kTopRight]);
    [tooltipController_ setImage:
        ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
            IDR_AUTOFILL_TOOLTIP_ICON).ToNSImage()];
    [tooltipController_ setMessage:
        base::SysUTF16ToNSString(notification->tooltip_text())];
    [[tooltipController_ view] setHidden:
        [[tooltipController_ message] length] == 0];

    [view setSubviews:@[ textview_, [tooltipController_ view] ]];
  }
  return self;
}

- (AutofillNotificationView*)notificationView {
  return base::mac::ObjCCastStrict<AutofillNotificationView>([self view]);
}

- (NSTextView*)textview {
  return textview_;
}

- (NSView*)tooltipView {
  return [tooltipController_ view];
}

- (NSSize)preferredSizeForWidth:(CGFloat)width {
  width -= 2 * chrome_style::kHorizontalPadding;
  if (![[tooltipController_ view] isHidden]) {
    width -= NSWidth([[tooltipController_ view] frame]) +
        chrome_style::kHorizontalPadding;
  }
  // TODO(isherman): Restore the DCHECK below once I figure out why it causes
  // unit tests to fail.
  //DCHECK_GT(width, 0);

  NSSize preferredSize;
  // This method is logically const. Hence, cache the original frame so that
  // it can be restored once the preferred size has been computed.
  NSRect frame = [textview_ frame];

  // Compute preferred size.
  [textview_ setFrameSize:NSMakeSize(width, frame.size.height)];
  [textview_ setVerticallyResizable:YES];
  [textview_ sizeToFit];
  preferredSize = [textview_ frame].size;

  // Restore original properties, since this method is logically const.
  [textview_ setFrame:frame];
  [textview_ setVerticallyResizable:NO];

  preferredSize.height += 2 * autofill::kNotificationPadding;
  return preferredSize;
}

- (NSSize)preferredSize {
  NOTREACHED();
  return NSZeroSize;
}

- (void)performLayout {
  NSRect bounds = [[self view] bounds];
  // Calculate the frame size, leaving room for padding around the notification,
  // as well as for the tooltip if it is visible.
  NSRect labelFrame = NSInsetRect(bounds,
                                 chrome_style::kHorizontalPadding,
                                 autofill::kNotificationPadding);
  NSView* tooltipView = [tooltipController_ view];
  if (![tooltipView isHidden]) {
    labelFrame.size.width -=
        NSWidth([tooltipView frame]) + chrome_style::kHorizontalPadding;
  }

  NSView* label = textview_.get();
  [label setFrame:labelFrame];

  if (![tooltipView isHidden]) {
    NSPoint tooltipOrigin =
        NSMakePoint(
            NSMaxX(labelFrame) + chrome_style::kHorizontalPadding,
            NSMidY(labelFrame) - (NSHeight([tooltipView frame]) / 2.0));
    [tooltipView setFrameOrigin:tooltipOrigin];
  }
}

- (BOOL)textView:(NSTextView *)textView
   clickedOnLink:(id)link
         atIndex:(NSUInteger)charIndex {
  delegate_->LinkClicked(linkURL_);
  return YES;
}

@end
