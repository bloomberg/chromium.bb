// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_pop_up_button.h"

#include <ApplicationServices/ApplicationServices.h>

#include "base/mac/scoped_nsobject.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"

@interface AutofillPopUpButton ()
- (void)didSelectItem:(id)sender;
@end

@implementation AutofillPopUpButton

@synthesize inputDelegate = inputDelegate_;

+ (Class)cellClass {
  return [AutofillPopUpCell class];
}

- (id)initWithFrame:(NSRect)frame pullsDown:(BOOL)pullsDown{
  if (self = [super initWithFrame:frame pullsDown:pullsDown]) {
    [self setTarget:self];
    [self setAction:@selector(didSelectItem:)];
  }
  return self;
}

- (BOOL)becomeFirstResponder {
  BOOL result = [super becomeFirstResponder];
  if (result && inputDelegate_)
    [inputDelegate_ fieldBecameFirstResponder:self];
  return result;
}

- (NSString*)fieldValue {
  return [[self cell] fieldValue];
}

- (void)setFieldValue:(NSString*)fieldValue {
  [[self cell] setFieldValue:fieldValue];
}

- (NSString*)validityMessage {
  return validityMessage_;
}

- (void)setValidityMessage:(NSString*)validityMessage {
  validityMessage_.reset([validityMessage copy]);
  [[self cell] setInvalid:[self invalid]];
  [self setNeedsDisplay:YES];
}

- (BOOL)invalid {
  return [validityMessage_ length] != 0;
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

- (void)didSelectItem:(id)sender {
  if (inputDelegate_)
    [inputDelegate_ didEndEditing:self];
}

@end


@implementation AutofillPopUpCell

@synthesize invalid = invalid_;
@synthesize defaultValue = defaultValue_;

// Draw a bezel that's highlighted.
- (void)drawBezelWithFrame:(NSRect)frame inView:(NSView*)controlView {
  if (invalid_) {
    CGContextRef context = static_cast<CGContextRef>(
        [[NSGraphicsContext currentContext] graphicsPort]);

    // Create a highlight-shaded bezel in a transparency layer.
    CGContextBeginTransparencyLayerWithRect(context, NSRectToCGRect(frame), 0);
    // 1. Draw bezel.
    [super drawBezelWithFrame:frame inView:controlView];

    // 2. Use that as stencil against solid color rect.
    [[NSColor redColor] set];
    NSRectFillUsingOperation(frame, NSCompositeSourceAtop);

    // 3. Composite the solid color bezel and the actual bezel.
    CGContextSetBlendMode(context, kCGBlendModePlusDarker);
    [super drawBezelWithFrame:frame inView:controlView];
    CGContextEndTransparencyLayer(context);
  } else {
    [super drawBezelWithFrame:frame inView:controlView];
  }
}

- (NSRect)drawTitle:(NSAttributedString*)title
          withFrame:(NSRect)frame
             inView:(NSView*)controlView {
  if (invalid_) {
    // Draw with a color that has high contrast against the custom background.
    base::scoped_nsobject<NSMutableAttributedString> coloredTitle(
        [[NSMutableAttributedString alloc] initWithAttributedString:title]);
    [coloredTitle addAttribute:NSForegroundColorAttributeName
                         value:[NSColor whiteColor]
                         range:NSMakeRange(0, [title length])];
    return [super drawTitle:coloredTitle withFrame:frame inView:controlView];
  } else {
    return [super drawTitle:title withFrame:frame inView:controlView];
  }
}

- (NSString*)fieldValue {
  if (![self selectedItem])
    return defaultValue_;
  return [self titleOfSelectedItem];
}

- (void)setFieldValue:(NSString*)fieldValue {
  [self selectItemWithTitle:fieldValue];
  if (![self selectedItem])
    [self selectItemWithTitle:defaultValue_];
  if (![self selectedItem])
    [self selectItemAtIndex:0];
}

@end
