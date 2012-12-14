// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/intents/web_intent_service_row_view_controller.h"

#include <cmath>

#import "chrome/browser/ui/chrome_style.h"
#import "chrome/browser/ui/constrained_window.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_button.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_control_utils.h"
#import "chrome/browser/ui/cocoa/flipped_view.h"
#import "chrome/browser/ui/cocoa/hyperlink_button_cell.h"
#import "chrome/browser/ui/cocoa/ratings_view.h"
#import "chrome/browser/ui/intents/web_intent_picker.h"
#include "grit/generated_resources.h"
#include "grit/google_chrome_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "skia/ext/skia_utils_mac.h"

namespace {

// Gets the size of the button with the given string.
NSSize GetSelectButtonSizeForStringId(int stringId) {
  scoped_nsobject<NSButton> button(
      [[ConstrainedWindowButton alloc] initWithFrame:NSZeroRect]);
  [button setTitle:l10n_util::GetNSStringWithFixup(stringId)];
  [button sizeToFit];
  return [button frame].size;
}

// Utility function to get the size of the select button. The button should have
// the same size whether it says "Select" or "Add to Chrome". This function
// calculates the button size with both strings and returns the maximum size.
NSSize GetSelectButtonSize() {
  static BOOL sShouldInitialize = YES;
  static NSSize sSelectButtonSize;

  if (sShouldInitialize) {
    sShouldInitialize = NO;
    NSSize size1 = GetSelectButtonSizeForStringId(
        IDS_INTENT_PICKER_SELECT_INTENT);
    NSSize size2 = GetSelectButtonSizeForStringId(
        IDS_INTENT_PICKER_INSTALL_EXTENSION);
    sSelectButtonSize.width = std::max(size1.width, size2.width);
    sSelectButtonSize.height = std::max(size1.height, size2.height);
  }

  return sSelectButtonSize;
}

}  // namespace

@interface WebIntentServiceRowViewController ()
// Common initialization for both the suggested and installed service row.
- (void)commonInitWithTitle:(NSString*)title
                     asLink:(BOOL)asLink
                buttonTitle:(NSString*)buttonTitle
                       icon:(NSImage*)icon;
@end

@implementation WebIntentServiceRowViewController

- (id)initSuggestedServiceRowWithTitle:(NSString*)title
                                  icon:(NSImage*)icon
                                rating:(double)rating {
  if ((self = [super init])) {
    NSString* buttonTitle = l10n_util::GetNSStringWithFixup(
        IDS_INTENT_PICKER_INSTALL_EXTENSION);
    [self commonInitWithTitle:title
                       asLink:YES
                  buttonTitle:buttonTitle
                         icon:icon];
    ratingsView_.reset([[RatingsView2 alloc] initWithRating:rating]);
    [[self view] addSubview:ratingsView_];
  }
  return self;
}

- (id)initInstalledServiceRowWithTitle:(NSString*)title
                                  icon:(NSImage*)icon {
  if ((self = [super init])) {
    NSString* buttonTitle = l10n_util::GetNSStringWithFixup(
        IDS_INTENT_PICKER_SELECT_INTENT);
    [self commonInitWithTitle:title
                       asLink:NO
                  buttonTitle:buttonTitle
                         icon:icon];
    ratingsView_.reset([[RatingsView2 alloc] initWithRating:0]);

    // Hide the ratings view so that it still takes up space and the
    // installed services titles line up with suggested services titles.
    [ratingsView_ setHidden:YES];
  }
  return self;
}

- (NSButton*)titleLinkButton {
  return titleLinkButton_;
}

- (NSButton*)selectButton {
  return selectButton_;
}

- (NSSize)minimumSizeForInnerWidth:(CGFloat)innerWidth {
  NSRect titleRect;
  if (titleTextField_.get()) {
    [titleTextField_ sizeToFit];
    titleRect = [titleTextField_ frame];
  } else {
    [titleLinkButton_ sizeToFit];
    titleRect = [titleLinkButton_ frame];
  }
  CGFloat width = WebIntentPicker::kServiceIconWidth +
                  WebIntentPicker::kIconTextPadding * 2 + NSWidth(titleRect);
  width += NSWidth([ratingsView_ frame]) +
           WebIntentPicker::kStarButtonPadding;
  width += GetSelectButtonSize().width;
  return NSMakeSize(width, WebIntentPicker::kServiceRowHeight);
}

- (void)layoutSubviewsWithinFrame:(NSRect)innerFrame {
  NSRect buttonRect;
  buttonRect.size = GetSelectButtonSize();
  buttonRect.origin.x = NSMaxX(innerFrame) - NSWidth(buttonRect);
  buttonRect.origin.y = roundf(NSMidY(innerFrame) - NSHeight(buttonRect) / 2.0);
  [selectButton_ setFrame:buttonRect];

  NSRect ratingsRect;
  ratingsRect.size = [ratingsView_ frame].size;
  ratingsRect.origin.x = NSMinX(buttonRect) - NSWidth(ratingsRect) -
                         WebIntentPicker::kStarButtonPadding;
  ratingsRect.origin.y =
      roundf(NSMidY(innerFrame) - NSHeight(ratingsRect) / 2.0);
  [ratingsView_ setFrame:ratingsRect];

  NSRect iconFrame;
  iconFrame.size.width = WebIntentPicker::kServiceIconWidth;
  iconFrame.size.height = WebIntentPicker::kServiceIconHeight;
  iconFrame.origin.x = NSMinX(innerFrame);
  iconFrame.origin.y = roundf(NSMidY(innerFrame) - NSHeight(iconFrame) / 2.0);
  [iconView_ setFrame:iconFrame];

  NSRect titleRect;
  if (titleTextField_.get()) {
    [titleTextField_ sizeToFit];
    titleRect = [titleTextField_ frame];
  } else {
    [titleLinkButton_ sizeToFit];
    titleRect = [titleLinkButton_ frame];
  }
  titleRect.origin.x = NSMaxX(iconFrame) + WebIntentPicker::kIconTextPadding;
  titleRect.origin.y = roundf(NSMidY(innerFrame) - NSHeight(titleRect) / 2.0);
  titleRect.size.width = NSMinX(ratingsRect) - NSMinX(titleRect) -
                         WebIntentPicker::kIconTextPadding;
  if (NSWidth(titleRect) < 0)
    titleRect.size.width = 0;
  if (titleTextField_.get())
    [titleTextField_ setFrame:titleRect];
  else
    [titleLinkButton_ setFrame:titleRect];
}

- (void)commonInitWithTitle:(NSString*)title
                     asLink:(BOOL)asLink
                buttonTitle:(NSString*)buttonTitle
                       icon:(NSImage*)icon {
  scoped_nsobject<NSView> view(
      [[FlippedView alloc] initWithFrame:NSZeroRect]);
  [self setView:view];

  if (asLink) {
    titleLinkButton_.reset(
        [[HyperlinkButtonCell buttonWithString:title] retain]);
    [[titleLinkButton_ cell] setUnderlineOnHover:YES];
    [[titleTextField_ cell] setLineBreakMode:NSLineBreakByTruncatingTail];
    [titleLinkButton_ sizeToFit];
    [[self view] addSubview:titleLinkButton_];
  } else {
    titleTextField_.reset([constrained_window::CreateLabel() retain]);
    [titleTextField_ setAttributedStringValue:
        constrained_window::GetAttributedLabelString(
            title,
            chrome_style::kTextFontStyle,
            NSNaturalTextAlignment,
            NSLineBreakByTruncatingTail)];
    [titleTextField_ sizeToFit];
    [[self view] addSubview:titleTextField_];
  }

  selectButton_.reset(
      [[ConstrainedWindowButton alloc] initWithFrame:NSZeroRect]);
  [selectButton_ setTitle:buttonTitle];
  [[self view] addSubview:selectButton_];

  iconView_.reset([[NSImageView alloc] initWithFrame:NSZeroRect]);
  [iconView_ setImage:icon];
  [iconView_ setImageFrameStyle:NSImageFrameNone];
  [[self view] addSubview:iconView_];
}

@end
