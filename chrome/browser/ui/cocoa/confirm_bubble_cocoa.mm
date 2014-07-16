// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/confirm_bubble_cocoa.h"

#include "base/strings/string16.h"
#include "chrome/browser/themes/theme_service.h"
#import "chrome/browser/ui/cocoa/confirm_bubble_controller.h"
#include "chrome/browser/ui/confirm_bubble.h"
#include "chrome/browser/ui/confirm_bubble_model.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMNSBezierPath+RoundRect.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/point.h"

// The width for the message text. We break lines so the specified message fits
// into this width.
const int kMaxMessageWidth = 400;

// The corner redius of this bubble view.
const int kBubbleCornerRadius = 3;

// The color for the border of this bubble view.
const float kBubbleWindowEdge = 0.7f;

// Constants used for layouting controls. These variables are copied from
// "ui/views/layout/layout_constants.h".
// Vertical spacing between a label and some control.
const int kLabelToControlVerticalSpacing = 8;

// Horizontal spacing between controls that are logically related.
const int kRelatedControlHorizontalSpacing = 8;

// Vertical spacing between controls that are logically related.
const int kRelatedControlVerticalSpacing = 8;

// Vertical spacing between the edge of the window and the
// top or bottom of a button.
const int kButtonVEdgeMargin = 6;

// Horizontal spacing between the edge of the window and the
// left or right of a button.
const int kButtonHEdgeMargin = 7;

namespace chrome {

void ShowConfirmBubble(gfx::NativeWindow window,
                       gfx::NativeView anchor_view,
                       const gfx::Point& origin,
                       ConfirmBubbleModel* model) {
  // Create a custom NSViewController that manages a bubble view, and add it to
  // a child to the specified |anchor_view|. This controller will be
  // automatically deleted when it loses first-responder status.
  ConfirmBubbleController* controller =
      [[ConfirmBubbleController alloc] initWithParent:anchor_view
                                               origin:origin.ToCGPoint()
                                                model:model];
  [anchor_view addSubview:[controller view]
               positioned:NSWindowAbove
               relativeTo:nil];
  [[anchor_view window] makeFirstResponder:[controller view]];
}

}  // namespace chrome

// An interface that is derived from NSTextView and does not accept
// first-responder status, i.e. a NSTextView-derived class that never becomes
// the first responder. When we click a NSTextView object, it becomes the first
// responder. Unfortunately, we delete the ConfirmBubbleCocoa object anytime
// when it loses first-responder status not to prevent disturbing other
// responders.
// To prevent text views in this ConfirmBubbleCocoa object from stealing the
// first-responder status, we use this view in the ConfirmBubbleCocoa object.
@interface ConfirmBubbleTextView : NSTextView
@end

@implementation ConfirmBubbleTextView

- (BOOL)acceptsFirstResponder {
  return NO;
}

@end

// Private Methods
@interface ConfirmBubbleCocoa (Private)
- (void)performLayout;
- (void)closeBubble;
@end

@implementation ConfirmBubbleCocoa

- (id)initWithParent:(NSView*)parent
          controller:(ConfirmBubbleController*)controller {
  // Create a NSView and set its width. We will set its position and height
  // after finish layouting controls in performLayout:.
  NSRect bounds =
      NSMakeRect(0, 0, kMaxMessageWidth + kButtonHEdgeMargin * 2, 0);
  if (self = [super initWithFrame:bounds]) {
    parent_ = parent;
    controller_ = controller;
    [self performLayout];
  }
  return self;
}

- (void)drawRect:(NSRect)dirtyRect {
  // Fill the background rectangle in white and draw its edge.
  NSRect bounds = [self bounds];
  bounds = NSInsetRect(bounds, 0.5, 0.5);
  NSBezierPath* border =
      [NSBezierPath gtm_bezierPathWithRoundRect:bounds
                            topLeftCornerRadius:kBubbleCornerRadius
                           topRightCornerRadius:kBubbleCornerRadius
                         bottomLeftCornerRadius:kBubbleCornerRadius
                        bottomRightCornerRadius:kBubbleCornerRadius];
  [[NSColor colorWithDeviceWhite:1.0f alpha:1.0f] set];
  [border fill];
  [[NSColor colorWithDeviceWhite:kBubbleWindowEdge alpha:1.0f] set];
  [border stroke];
}

// An NSResponder method.
- (BOOL)resignFirstResponder {
  // We do not only accept this request but also close this bubble when we are
  // asked to resign the first responder. This bubble should be displayed only
  // while it is the first responder.
  [self closeBubble];
  return YES;
}

// NSControl action handlers. These handlers are called when we click a cancel
// button, a close icon, and an OK button, respectively.
- (IBAction)cancel:(id)sender {
  [controller_ cancel];
  [self closeBubble];
}

- (IBAction)close:(id)sender {
  [self closeBubble];
}

- (IBAction)ok:(id)sender {
  [controller_ accept];
  [self closeBubble];
}

// An NSTextViewDelegate method. This function is called when we click a link in
// this bubble.
- (BOOL)textView:(NSTextView*)textView
   clickedOnLink:(id)link
         atIndex:(NSUInteger)charIndex {
  [controller_ linkClicked];
  [self closeBubble];
  return YES;
}

// Initializes controls specified by the ConfirmBubbleModel object and layouts
// them into this bubble. This function retrieves text and images from the
// ConfirmBubbleModel object (via the ConfirmBubbleController object) and
// layouts them programmatically. This function layouts controls in the botom-up
// order since NSView uses bottom-up coordinate.
- (void)performLayout {
  NSRect frameRect = [self frame];

  // Add the ok button and the cancel button to the first row if we have either
  // of them.
  CGFloat left = kButtonHEdgeMargin;
  CGFloat right = NSWidth(frameRect) - kButtonHEdgeMargin;
  CGFloat bottom = kButtonVEdgeMargin;
  CGFloat height = 0;
  if ([controller_ hasOkButton]) {
    okButton_.reset([[NSButton alloc]
        initWithFrame:NSMakeRect(0, bottom, 0, 0)]);
    [okButton_.get() setBezelStyle:NSRoundedBezelStyle];
    [okButton_.get() setTitle:[controller_ okButtonText]];
    [okButton_.get() setTarget:self];
    [okButton_.get() setAction:@selector(ok:)];
    [okButton_.get() sizeToFit];
    NSRect okButtonRect = [okButton_.get() frame];
    right -= NSWidth(okButtonRect);
    okButtonRect.origin.x = right;
    [okButton_.get() setFrame:okButtonRect];
    [self addSubview:okButton_.get()];
    height = std::max(height, NSHeight(okButtonRect));
  }
  if ([controller_ hasCancelButton]) {
    cancelButton_.reset([[NSButton alloc]
        initWithFrame:NSMakeRect(0, bottom, 0, 0)]);
    [cancelButton_.get() setBezelStyle:NSRoundedBezelStyle];
    [cancelButton_.get() setTitle:[controller_ cancelButtonText]];
    [cancelButton_.get() setTarget:self];
    [cancelButton_.get() setAction:@selector(cancel:)];
    [cancelButton_.get() sizeToFit];
    NSRect cancelButtonRect = [cancelButton_.get() frame];
    right -= NSWidth(cancelButtonRect) + kButtonHEdgeMargin;
    cancelButtonRect.origin.x = right;
    [cancelButton_.get() setFrame:cancelButtonRect];
    [self addSubview:cancelButton_.get()];
    height = std::max(height, NSHeight(cancelButtonRect));
  }

  // Add the message label (and the link label) to the second row.
  left = kButtonHEdgeMargin;
  right = NSWidth(frameRect);
  bottom += height + kRelatedControlVerticalSpacing;
  height = 0;
  messageLabel_.reset([[ConfirmBubbleTextView alloc]
      initWithFrame:NSMakeRect(left, bottom, kMaxMessageWidth, 0)]);
  NSString* messageText = [controller_ messageText];
  NSMutableDictionary* attributes = [NSMutableDictionary dictionary];
  base::scoped_nsobject<NSMutableAttributedString> attributedMessage(
      [[NSMutableAttributedString alloc] initWithString:messageText
                                             attributes:attributes]);
  NSString* linkText = [controller_ linkText];
  if (linkText) {
    base::scoped_nsobject<NSAttributedString> whiteSpace(
        [[NSAttributedString alloc] initWithString:@" "]);
    [attributedMessage.get() appendAttributedString:whiteSpace.get()];
    [attributes setObject:[NSString string]
                   forKey:NSLinkAttributeName];
    base::scoped_nsobject<NSAttributedString> attributedLink(
        [[NSAttributedString alloc] initWithString:linkText
                                        attributes:attributes]);
    [attributedMessage.get() appendAttributedString:attributedLink.get()];
  }
  [[messageLabel_.get() textStorage] setAttributedString:attributedMessage];
  [messageLabel_.get() setHorizontallyResizable:NO];
  [messageLabel_.get() setVerticallyResizable:YES];
  [messageLabel_.get() setEditable:NO];
  [messageLabel_.get() setDrawsBackground:NO];
  [messageLabel_.get() setDelegate:self];
  [messageLabel_.get() sizeToFit];
  height = NSHeight([messageLabel_.get() frame]);
  [self addSubview:messageLabel_.get()];

  // Add the icon and the title label to the third row.
  left = kButtonHEdgeMargin;
  right = NSWidth(frameRect);
  bottom += height + kLabelToControlVerticalSpacing;
  height = 0;
  NSImage* iconImage = [controller_ icon];
  if (iconImage) {
    icon_.reset([[NSImageView alloc] initWithFrame:NSMakeRect(
        left, bottom, [iconImage size].width, [iconImage size].height)]);
    [icon_.get() setImage:iconImage];
    [self addSubview:icon_.get()];
    left += NSWidth([icon_.get() frame]) + kRelatedControlHorizontalSpacing;
    height = std::max(height, NSHeight([icon_.get() frame]));
  }
  titleLabel_.reset([[NSTextView alloc]
      initWithFrame:NSMakeRect(left, bottom, right - left, 0)]);
  [titleLabel_.get() setString:[controller_ title]];
  [titleLabel_.get() setHorizontallyResizable:NO];
  [titleLabel_.get() setVerticallyResizable:YES];
  [titleLabel_.get() setEditable:NO];
  [titleLabel_.get() setSelectable:NO];
  [titleLabel_.get() setDrawsBackground:NO];
  [titleLabel_.get() sizeToFit];
  [self addSubview:titleLabel_.get()];
  height = std::max(height, NSHeight([titleLabel_.get() frame]));

  // Adjust the frame rectangle of this bubble so we can show all controls.
  NSRect parentRect = [parent_ frame];
  frameRect.size.height = bottom + height + kButtonVEdgeMargin;
  frameRect.origin.x = (NSWidth(parentRect) - NSWidth(frameRect)) / 2;
  frameRect.origin.y = NSHeight(parentRect) - NSHeight(frameRect);
  [self setFrame:frameRect];
}

// Closes this bubble and releases all resources. This function just puts the
// owner ConfirmBubbleController object to the current autorelease pool. (This
// view will be deleted when the owner object is deleted.)
- (void)closeBubble {
  [self removeFromSuperview];
  [controller_ autorelease];
  parent_ = nil;
  controller_ = nil;
}

@end

@implementation ConfirmBubbleCocoa (ExposedForUnitTesting)

- (void)clickOk {
  [self ok:self];
}

- (void)clickCancel {
  [self cancel:self];
}

- (void)clickLink {
  [self textView:messageLabel_.get() clickedOnLink:nil atIndex:0];
}

@end
