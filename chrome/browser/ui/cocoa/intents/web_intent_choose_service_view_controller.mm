// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/intents/web_intent_choose_service_view_controller.h"

#include <cmath>

#import "chrome/browser/ui/chrome_style.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_control_utils.h"
#import "chrome/browser/ui/cocoa/flipped_view.h"
#import "chrome/browser/ui/cocoa/hyperlink_button_cell.h"
#import "chrome/browser/ui/constrained_window.h"
#import "chrome/browser/ui/intents/web_intent_picker.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "skia/ext/skia_utils_mac.h"
#include "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/l10n/l10n_util_mac.h"

@interface WebIntentChooseServiceViewController ()
// Resizes the title and message text fields to fit the given width.
- (void)resizeTextFieldsToWidth:(CGFloat)width;
@end

@implementation WebIntentChooseServiceViewController

- (id)init {
  if ((self = [super init])) {
    scoped_nsobject<NSView> view(
        [[FlippedView alloc] initWithFrame:NSZeroRect]);
    [self setView:view];

    titleTextField_.reset([constrained_window::CreateLabel() retain]);
    [[self view] addSubview:titleTextField_];

    messageTextField_.reset([constrained_window::CreateLabel() retain]);
    [[self view] addSubview:messageTextField_];

    separator_.reset([[NSBox alloc] initWithFrame:NSZeroRect]);
    [separator_ setBoxType:NSBoxCustom];
    [separator_ setBorderColor:gfx::SkColorToCalibratedNSColor(
        chrome_style::GetSeparatorColor())];
    [[self view] addSubview:separator_];

    NSString* moreServicesTitle = l10n_util::GetNSStringWithFixup(
          IDS_FIND_MORE_INTENT_HANDLER_MESSAGE);
    showMoreServicesButton_.reset(
        [[HyperlinkButtonCell buttonWithString:moreServicesTitle] retain]);
    [[showMoreServicesButton_ cell] setUnderlineOnHover:YES];
    [[showMoreServicesButton_ cell] setTextColor:
        gfx::SkColorToCalibratedNSColor(chrome_style::GetLinkColor())];
    [showMoreServicesButton_ sizeToFit];
    [[self view] addSubview:showMoreServicesButton_];

    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    NSImage* moreServicesIcon =
        rb.GetNativeImageNamed(IDR_WEBSTORE_ICON_16).ToNSImage();
    NSRect moreServicesIconRect = NSZeroRect;
    moreServicesIconRect.size = [moreServicesIcon size];
    moreServicesIconView_.reset(
        [[NSImageView alloc] initWithFrame:moreServicesIconRect]);
    [moreServicesIconView_ setImage:moreServicesIcon];
    [moreServicesIconView_ setImageFrameStyle:NSImageFrameNone];
    [[self view] addSubview:moreServicesIconView_];
  }
  return self;
}

- (NSButton*)showMoreServicesButton {
  return showMoreServicesButton_;
}

- (void)setTitle:(NSString*)title {
  [titleTextField_ setAttributedStringValue:
      constrained_window::GetAttributedLabelString(
          title,
          chrome_style::kBoldTextFontStyle,
          NSNaturalTextAlignment,
          NSLineBreakByWordWrapping)];
}

- (void)setMessage:(NSString*)message {
  [messageTextField_ setAttributedStringValue:
      constrained_window::GetAttributedLabelString(
          message,
          chrome_style::kTextFontStyle,
          NSNaturalTextAlignment,
          NSLineBreakByWordWrapping)];
}

- (void)setRows:(NSArray*)rows {
  for (NSViewController* row in rows_.get())
    [[row view] removeFromSuperview];
  rows_.reset([rows retain]);
  for (NSViewController* row in rows_.get())
    [[self view] addSubview:[row view]];
}

- (NSArray*)rows {
  return rows_;
}

- (NSSize)minimumSizeForInnerWidth:(CGFloat)innerWidth {
  [self resizeTextFieldsToWidth:innerWidth];
  CGFloat height = NSHeight([titleTextField_ frame]);
  if (messageTextField_)
    height += NSHeight([messageTextField_ frame]);

  height += WebIntentPicker::kHeaderSeparatorPaddingTop +
            WebIntentPicker::kHeaderSeparatorPaddingBottom;

  CGFloat width = innerWidth;
  for (WebIntentViewController* row in rows_.get()) {
    NSSize size = [row minimumSizeForInnerWidth:innerWidth];
    height += WebIntentPicker::kServiceRowHeight;
    width = std::max(width, size.width);
  }

  height += WebIntentPicker::kServiceRowHeight;
  CGFloat moreServicesWidth = NSWidth([moreServicesIconView_ frame]) +
                              WebIntentPicker::kIconTextPadding +
                              NSWidth([showMoreServicesButton_ frame]);
  width = std::max(width, moreServicesWidth);

  return NSMakeSize(width, height);
}

- (void)layoutSubviewsWithinFrame:(NSRect)innerFrame {
  [self resizeTextFieldsToWidth:NSWidth(innerFrame)];

  NSRect titleFrame = [titleTextField_ frame];
  titleFrame.origin = innerFrame.origin;
  [titleTextField_ setFrame:titleFrame];
  CGFloat yPos = NSMaxY(titleFrame);

  if (messageTextField_) {
    NSRect messageFrame = [messageTextField_ frame];
    messageFrame.origin.x = NSMinX(innerFrame);
    messageFrame.origin.y = yPos;
    [messageTextField_ setFrame:messageFrame];
    yPos = NSMaxY(messageFrame);
  }

  NSRect separatorFrame = NSMakeRect(
      0,
      yPos + WebIntentPicker::kHeaderSeparatorPaddingTop,
      NSWidth([[self view] bounds]),
      1.0);
  [separator_ setFrame:separatorFrame];

  yPos = NSMaxY(separatorFrame) +
         WebIntentPicker::kHeaderSeparatorPaddingBottom;

  for (WebIntentViewController* row in rows_.get()) {
    NSRect rowRect = NSMakeRect(
        NSMinX(innerFrame),
        yPos,
        NSWidth(innerFrame),
        WebIntentPicker::kServiceRowHeight);
    [[row view] setFrame:rowRect];
    [row layoutSubviewsWithinFrame:[[row view] bounds]];
    yPos = NSMaxY(rowRect);
  }

  NSRect moreRowRect = NSMakeRect(NSMinX(innerFrame),
                                  yPos,
                                  NSWidth(innerFrame),
                                  WebIntentPicker::kServiceRowHeight);
  NSRect moreIconRect = [moreServicesIconView_ frame];
  moreIconRect.origin.x = NSMinX(moreRowRect);
  moreIconRect.origin.y =
      roundf(NSMidY(moreRowRect) - NSHeight(moreIconRect) / 2.0);
  [moreServicesIconView_ setFrame:moreIconRect];

  NSRect moreButtonRect = [showMoreServicesButton_ frame];
  moreButtonRect.origin.x = NSMaxX(moreIconRect) +
                            WebIntentPicker::kIconTextPadding;
  moreButtonRect.origin.y = roundf(NSMidY(moreRowRect) -
                                   NSHeight(moreButtonRect) / 2.0);
  moreButtonRect.size.width = NSWidth(moreRowRect) - NSMinX(moreButtonRect);
  [showMoreServicesButton_ setFrame:moreButtonRect];
}

- (void)resizeTextFieldsToWidth:(CGFloat)width {
  NSRect frame = NSZeroRect;
  frame.size.width = width;

  [messageTextField_ setFrame:frame];
  if ([[messageTextField_ stringValue] length]) {
    [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:
        messageTextField_];
  }

  frame.size.width -= chrome_style::GetCloseButtonSize() +
                      WebIntentPicker::kIconTextPadding;
  [titleTextField_ setFrame:frame];
  [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:titleTextField_];
}

@end
