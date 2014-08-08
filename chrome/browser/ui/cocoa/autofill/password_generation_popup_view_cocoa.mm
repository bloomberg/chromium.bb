// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/password_generation_popup_view_cocoa.h"

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"
#include "chrome/browser/ui/autofill/popup_constants.h"
#include "chrome/browser/ui/chrome_style.h"
#include "chrome/browser/ui/cocoa/autofill/password_generation_popup_view_bridge.h"
#import "chrome/browser/ui/cocoa/hyperlink_text_view.h"
#import "chrome/browser/ui/cocoa/l10n_util.h"
#include "components/autofill/core/browser/popup_item_ids.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/point.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/text_constants.h"

using autofill::AutofillPopupView;
using autofill::PasswordGenerationPopupView;
using base::scoped_nsobject;

namespace {

NSColor* DividerColor() {
  return gfx::SkColorToCalibratedNSColor(
      PasswordGenerationPopupView::kDividerColor);
}

NSColor* HelpTextBackgroundColor() {
  return gfx::SkColorToCalibratedNSColor(
      PasswordGenerationPopupView::kExplanatoryTextBackgroundColor);
}

NSColor* HelpTextColor() {
  return gfx::SkColorToCalibratedNSColor(
      PasswordGenerationPopupView::kExplanatoryTextColor);
}

NSColor* HelpLinkColor() {
  return gfx::SkColorToCalibratedNSColor(chrome_style::GetLinkColor());
}

}  // namespace

@implementation PasswordGenerationPopupViewCocoa

#pragma mark Initialisers

- (id)initWithFrame:(NSRect)frame {
  NOTREACHED();
  return nil;
}

- (id)initWithController:
    (autofill::PasswordGenerationPopupController*)controller
                   frame:(NSRect)frame {
  if (self = [super initWithDelegate:controller frame:frame]) {
    controller_ = controller;

    passwordField_ = [self textFieldWithText:controller_->password()
                                       color:[self nameColor]
                                   alignment:NSLeftTextAlignment];
    [self addSubview:passwordField_];

    passwordSubtextField_ = [self textFieldWithText:controller_->SuggestedText()
                                              color:[self subtextColor]
                                          alignment:NSRightTextAlignment];
    [self addSubview:passwordSubtextField_];

    scoped_nsobject<HyperlinkTextView> helpTextView(
        [[HyperlinkTextView alloc] initWithFrame:NSZeroRect]);
    [helpTextView setMessage:base::SysUTF16ToNSString(controller_->HelpText())
                    withFont:[self textFont]
                messageColor:HelpTextColor()];
    [helpTextView addLinkRange:controller_->HelpTextLinkRange().ToNSRange()
                      withName:@""
                     linkColor:HelpLinkColor()];
    [helpTextView setDelegate:self];
    [[helpTextView textContainer] setLineFragmentPadding:0.0f];
    [helpTextView setVerticallyResizable:YES];
    [self addSubview:helpTextView];
    helpTextView_ = helpTextView.get();
  }

  return self;
}

#pragma mark NSView implementation:

- (void)drawRect:(NSRect)dirtyRect {
  // If the view is in the process of being destroyed, don't bother drawing.
  if (!controller_)
    return;

  [self drawBackgroundAndBorder];

  if (controller_->password_selected()) {
    // Draw a highlight under the suggested password.
    NSRect highlightBounds = [self passwordBounds];
    [[self highlightColor] set];
    [NSBezierPath fillRect:highlightBounds];
  }

  // Render the background of the help text.
  [HelpTextBackgroundColor() set];
  [NSBezierPath fillRect:[self helpBounds]];

  // Render the divider.
  [DividerColor() set];
  [NSBezierPath fillRect:[self dividerBounds]];
}

#pragma mark Public API:

- (void)updateBoundsAndRedrawPopup {
  [self positionView:passwordField_ inRect:[self passwordBounds]];
  [self positionView:passwordSubtextField_ inRect:[self passwordBounds]];
  [self positionView:helpTextView_ inRect:[self helpBounds]];

  [super updateBoundsAndRedrawPopup];
}

- (void)controllerDestroyed {
  controller_ = NULL;
  [super delegateDestroyed];
}

#pragma mark NSTextViewDelegate implementation:

- (BOOL)textView:(NSTextView*)textView
   clickedOnLink:(id)link
         atIndex:(NSUInteger)charIndex {
  controller_->OnSavedPasswordsLinkClicked();
  return YES;
}

#pragma mark Private helpers:

- (NSTextField*)textFieldWithText:(const base::string16&)text
                            color:(NSColor*)color
                        alignment:(NSTextAlignment)alignment {
  scoped_nsobject<NSMutableParagraphStyle> paragraphStyle(
      [[NSMutableParagraphStyle alloc] init]);
  [paragraphStyle setAlignment:alignment];

  NSDictionary* textAttributes = @{
    NSFontAttributeName : [self textFont],
    NSForegroundColorAttributeName : color,
    NSParagraphStyleAttributeName : paragraphStyle
  };

  scoped_nsobject<NSAttributedString> attributedString(
      [[NSAttributedString alloc]
          initWithString:base::SysUTF16ToNSString(text)
              attributes:textAttributes]);

  NSTextField* textField =
      [[[NSTextField alloc] initWithFrame:NSZeroRect] autorelease];
  [textField setAttributedStringValue:attributedString];
  [textField setEditable:NO];
  [textField setSelectable:NO];
  [textField setDrawsBackground:NO];
  [textField setBezeled:NO];
  return textField;
}

- (void)positionView:(NSView*)view inRect:(NSRect)bounds {
  NSRect frame = NSInsetRect(bounds, controller_->kHorizontalPadding, 0);
  [view setFrame:frame];

  // Center the text vertically within the bounds.
  NSSize delta = cocoa_l10n_util::WrapOrSizeToFit(view);
  [view setFrameOrigin:
      NSInsetRect(frame, 0, floor(-delta.height/2)).origin];
}

- (NSRect)passwordBounds {
  return NSZeroRect;
}

- (NSRect)helpBounds {
  return NSZeroRect;
}

- (NSRect)dividerBounds {
  return NSZeroRect;
}

- (NSFont*)textFont {
  return ResourceBundle::GetSharedInstance().GetFontList(
      ResourceBundle::SmallFont).GetPrimaryFont().GetNativeFont();
}

@end
