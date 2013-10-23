// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_textfield.h"

#include <algorithm>
#include <cmath>

#include "base/logging.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"

namespace {

const CGFloat kGap = 6.0;  // gap between icon and text.
const CGFloat kMinimumHeight = 27.0;  // Enforced minimum height for text cells.

}  // namespace

@interface AutofillTextFieldCell (Internal)

- (NSRect)iconFrameForFrame:(NSRect)frame;
- (NSRect)textFrameForFrame:(NSRect)frame;

@end

@implementation AutofillTextField

@synthesize delegate = delegate_;

+ (Class)cellClass {
  return [AutofillTextFieldCell class];
}

- (id)initWithFrame:(NSRect)frame {
  if (self = [super initWithFrame:frame])
    [super setDelegate:self];
  return self;
}

- (BOOL)becomeFirstResponder {
  BOOL result = [super becomeFirstResponder];
  if (result && delegate_)
    [delegate_ fieldBecameFirstResponder:self];
  return result;
}

- (void)controlTextDidEndEditing:(NSNotification*)notification {
  if (delegate_)
    [delegate_ didEndEditing:self];
}

- (void)controlTextDidChange:(NSNotification*)aNotification {
  if (delegate_)
    [delegate_ didChange:self];
}

- (NSString*)fieldValue {
  return [[self cell] fieldValue];
}

- (void)setFieldValue:(NSString*)fieldValue {
  [[self cell] setFieldValue:fieldValue];
}

- (NSString*)defaultValue {
  return [[self cell] defaultValue];
}

- (void)setDefaultValue:(NSString*)defaultValue {
  [[self cell] setDefaultValue:defaultValue];
}

- (BOOL)isDefault {
  return [[[self cell] fieldValue] isEqualToString:[[self cell] defaultValue]];
}

- (NSString*)validityMessage {
  return validityMessage_;
}

- (void)setValidityMessage:(NSString*)validityMessage {
  validityMessage_.reset([validityMessage copy]);
  [[self cell] setInvalid:[self invalid]];
}

- (BOOL)invalid {
  return [validityMessage_ length] != 0;
}

@end

@implementation AutofillTextFieldCell

@synthesize invalid = invalid_;
@synthesize defaultValue = defaultValue_;

- (void)setInvalid:(BOOL)invalid {
  invalid_ = invalid;
  [[self controlView] setNeedsDisplay:YES];
}

- (NSImage*) icon{
  return icon_;
}

- (void)setIcon:(NSImage*) icon {
  icon_.reset([icon retain]);
  [[self controlView] setNeedsDisplay:YES];
}

- (NSString*)fieldValue {
  return [self stringValue];
}

- (void)setFieldValue:(NSString*)fieldValue {
  [self setStringValue:fieldValue];
}

- (NSRect)textFrameForFrame:(NSRect)frame {
  // Ensure text height is original cell height, and the text frame is centered
  // vertically in the cell frame.
  NSSize originalSize = [super cellSize];
  if (originalSize.height < NSHeight(frame)) {
    CGFloat delta = NSHeight(frame) - originalSize.height;
    frame.origin.y += std::floor(delta / 2.0);
    frame.size.height -= delta;
  }
  DCHECK_EQ(originalSize.height, NSHeight(frame));

  if (icon_) {
    NSRect textFrame, iconFrame;
    NSDivideRect(frame, &iconFrame, &textFrame,
                 kGap + [icon_ size].width, NSMaxXEdge);
    return textFrame;
  }
  return frame;
}

- (NSRect)iconFrameForFrame:(NSRect)frame {
  NSRect iconFrame;
  if (icon_) {
    NSRect textFrame;
    NSDivideRect(frame, &iconFrame, &textFrame,
                 kGap + [icon_ size].width, NSMaxXEdge);
  }
  return iconFrame;
}

- (NSSize)cellSize {
  NSSize cellSize = [super cellSize];

  if (icon_) {
    NSSize iconSize = [icon_ size];
    cellSize.width += kGap + iconSize.width;
    cellSize.height = std::max(cellSize.height, iconSize.height);
  }
  cellSize.height = std::max(cellSize.height, kMinimumHeight);
  return cellSize;
}

- (void)editWithFrame:(NSRect)cellFrame
               inView:(NSView *)controlView
               editor:(NSText *)editor
             delegate:(id)delegate
                event:(NSEvent *)event {
  [super editWithFrame:[self textFrameForFrame:cellFrame]
                inView:controlView
                editor:editor
              delegate:delegate
                 event:event];
}

- (void)selectWithFrame:(NSRect)cellFrame
                 inView:(NSView *)controlView
                 editor:(NSText *)editor
               delegate:(id)delegate
                  start:(NSInteger)start
                 length:(NSInteger)length {
  [super selectWithFrame:[self textFrameForFrame:cellFrame]
                  inView:controlView
                  editor:editor
                delegate:delegate
                   start:start
                  length:length];
}

- (void)drawInteriorWithFrame:(NSRect)cellFrame inView:(NSView*)controlView {
  NSRect textFrame = [self textFrameForFrame:cellFrame];
  [super drawInteriorWithFrame:textFrame inView:controlView];
}

- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView*)controlView {
  [super drawWithFrame:cellFrame inView:controlView];

  if (icon_) {
    NSRect iconFrame = [self iconFrameForFrame:cellFrame];
    iconFrame.size = [icon_ size];
    iconFrame.origin.y +=
        roundf((NSHeight(cellFrame) - NSHeight(iconFrame)) / 2.0);
    [icon_ drawInRect:iconFrame
             fromRect:NSZeroRect
            operation:NSCompositeSourceOver
             fraction:1.0
       respectFlipped:YES
                hints:nil];
  }

  if (invalid_) {
    gfx::ScopedNSGraphicsContextSaveGState state;

    // Render red border for invalid fields.
    [[NSColor colorWithDeviceRed:1.0 green:0.0 blue:0.0 alpha:1.0] setStroke];
    [[NSBezierPath bezierPathWithRect:NSInsetRect(cellFrame, 0.5, 0.5)] stroke];
  }
}

@end
