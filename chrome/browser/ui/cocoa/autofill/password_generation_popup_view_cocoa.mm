// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/password_generation_popup_view_cocoa.h"

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"
#include "chrome/browser/ui/autofill/popup_constants.h"
#include "chrome/browser/ui/cocoa/autofill/password_generation_popup_view_bridge.h"
#include "components/autofill/core/browser/popup_item_ids.h"
#include "grit/ui_resources.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/text_constants.h"

using autofill::AutofillPopupView;
using autofill::PasswordGenerationPopupView;

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

}  // namespace

@implementation PasswordGenerationPopupViewCocoa

#pragma mark -
#pragma mark Initialisers

- (id)initWithFrame:(NSRect)frame {
  NOTREACHED();
  return nil;
}

- (id)initWithController:
    (autofill::PasswordGenerationPopupController*)controller
                   frame:(NSRect)frame {
  self = [super initWithDelegate:controller frame:frame];
  if (self)
    controller_ = controller;

  return self;
}

#pragma mark -
#pragma mark NSView implementation:

- (void)drawRect:(NSRect)dirtyRect {
  // If the view is in the process of being destroyed, don't bother drawing.
  if (!controller_)
    return;

  [self drawBackgroundAndBorder];

  NSRect bounds = [self bounds];
  bounds.origin.y += autofill::kPopupBorderThickness;

  if (controller_->password_selected()) {
    // Draw a highlight under the suggested password.
    NSRect highlightBounds =
        NSRectFromCGRect(controller_->password_bounds().ToCGRect());
    highlightBounds.origin.y +=
        PasswordGenerationPopupView::kPasswordVerticalInset;
    highlightBounds.size.height -=
        2 * PasswordGenerationPopupView::kPasswordVerticalInset;
    [[self highlightColor] set];
    [NSBezierPath fillRect:highlightBounds];
  }

  NSFont* font = controller_->font_list().GetPrimaryFont().GetNativeFont();
  NSRect passwordBounds =
      NSRectFromCGRect(controller_->password_bounds().ToCGRect());

  BOOL isRTL = NO;  // TODO(dubroy): Implement RTL support.
  [self drawText:base::SysUTF16ToNSString(controller_->password())
        withFont:font
           color:[self nameColor]
          bounds:passwordBounds
       alignment:isRTL ? gfx::ALIGN_RIGHT : gfx::ALIGN_LEFT];

  [self drawText:base::SysUTF16ToNSString(controller_->SuggestedText())
        withFont:font
           color:[self subtextColor]
          bounds:passwordBounds
       alignment:isRTL ? gfx::ALIGN_LEFT : gfx::ALIGN_RIGHT];

  // Render the background of the help text.
  NSRect helpBounds =
      NSRectFromCGRect(controller_->help_bounds().ToCGRect());
  [HelpTextBackgroundColor() set];
  [NSBezierPath fillRect:helpBounds];

  // Render the divider.
  NSRect helpBorder = helpBounds;
  helpBorder.size.height = 1;
  [DividerColor() set];
  [NSBezierPath fillRect:helpBorder];

  // Adjust |helpBounds| so that the divider is not included when calculating
  // where the text should be rendered.
  helpBounds.origin.x += 1;
  helpBounds.size.height -= 1;

  // Render the help text.
  [self drawText:base::SysUTF16ToNSString(controller_->HelpText())
        withFont:font
           color:HelpTextColor()
          bounds:helpBounds
       alignment:isRTL ? gfx::ALIGN_RIGHT : gfx::ALIGN_LEFT];
}

- (void)drawText:(NSString*)text
        withFont:(NSFont*)font
           color:(id)color
          bounds:(NSRect)bounds
       alignment:(gfx::HorizontalAlignment)alignment {
  NSDictionary* textAttributes = @{
    NSFontAttributeName: font,
    NSForegroundColorAttributeName: color,
  };
  // Adjust horizontal padding before measuring.
  bounds.size.width -= 2 * controller_->kHorizontalPadding;
  bounds.origin.x += controller_->kHorizontalPadding;

  NSSize textSize =
      [text boundingRectWithSize:bounds.size
                         options:NSStringDrawingUsesLineFragmentOrigin
                      attributes:textAttributes].size;

  // Center the text vertically within the bounds.
  bounds.origin.y = NSMinY(bounds) + (NSHeight(bounds) - textSize.height) / 2;

  if (alignment == gfx::ALIGN_RIGHT)
    bounds.origin.x += NSWidth(bounds) - textSize.width;

  [text drawInRect:bounds withAttributes:textAttributes];
}

#pragma mark -
#pragma mark Public API:

- (void)controllerDestroyed {
  controller_ = NULL;
  [super delegateDestroyed];
}

@end
