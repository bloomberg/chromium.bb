// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/tab_contents/sad_tab_view.h"

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#import "chrome/browser/ui/cocoa/hyperlink_button_cell.h"
#import "chrome/browser/ui/cocoa/hyperlink_text_view.h"
#include "chrome/browser/ui/cocoa/tab_contents/sad_tab_controller.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#import "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

// Offset above vertical middle of page where contents of page start.
static const CGFloat kSadTabOffset = -64;
// Padding between icon and title.
static const CGFloat kIconTitleSpacing = 20;
// Padding between title and message.
static const CGFloat kTitleMessageSpacing = 15;
// Padding between message and link.
static const CGFloat kMessageLinkSpacing = 15;
// Paddings on left and right of page.
static const CGFloat kTabHorzMargin = 13;

@implementation SadTabView

- (void)awakeFromNib {
  // Load resource for image and set it.
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  NSImage* image = rb.GetNativeImageNamed(IDR_SAD_TAB).ToNSImage();
  [image_ setImage:image];

  // Set font for title.
  NSFont* titleFont = [NSFont boldSystemFontOfSize:[NSFont systemFontSize]];
  [title_ setFont:titleFont];

  // Set font for message.
  NSFont* messageFont = [NSFont systemFontOfSize:[NSFont smallSystemFontSize]];
  [message_ setFont:messageFont];

  DCHECK(controller_);
  [self initializeHelpText];

  // Initialize background color.
  NSColor* backgroundColor = [[NSColor colorWithCalibratedRed:(35.0f/255.0f)
                                                        green:(48.0f/255.0f)
                                                         blue:(64.0f/255.0f)
                                                        alpha:1.0] retain];
  backgroundColor_.reset(backgroundColor);
}

- (void)drawRect:(NSRect)dirtyRect {
  // Paint background.
  [backgroundColor_ set];
  NSRectFill(dirtyRect);
}

- (void)resizeSubviewsWithOldSize:(NSSize)oldSize {
  NSRect newBounds = [self bounds];
  CGFloat maxWidth = NSWidth(newBounds) - (kTabHorzMargin * 2);
  BOOL callSizeToFit = (messageSize_.width == 0);

  // Set new frame origin for image.
  NSRect iconFrame = [image_ frame];
  CGFloat iconX = (maxWidth - NSWidth(iconFrame)) / 2;
  CGFloat iconY =
      MIN(((NSHeight(newBounds) - NSHeight(iconFrame)) / 2) - kSadTabOffset,
          NSHeight(newBounds) - NSHeight(iconFrame));
  iconX = floor(iconX);
  iconY = floor(iconY);
  [image_ setFrameOrigin:NSMakePoint(iconX, iconY)];

  // Set new frame origin for title.
  if (callSizeToFit)
    [title_ sizeToFit];
  NSRect titleFrame = [title_ frame];
  CGFloat titleX = (maxWidth - NSWidth(titleFrame)) / 2;
  CGFloat titleY = iconY - kIconTitleSpacing - NSHeight(titleFrame);
  [title_ setFrameOrigin:NSMakePoint(titleX, titleY)];

  // Set new frame for message, wrapping or unwrapping the text if necessary.
  if (callSizeToFit) {
    [message_ sizeToFit];
    messageSize_ = [message_ frame].size;
  }
  NSRect messageFrame = [message_ frame];
  if (messageSize_.width > maxWidth) {  // Need to wrap message.
    [message_ setFrameSize:NSMakeSize(maxWidth, messageSize_.height)];
    CGFloat heightChange =
        [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:message_];
    messageFrame.size.width = maxWidth;
    messageFrame.size.height = messageSize_.height + heightChange;
    messageFrame.origin.x = kTabHorzMargin;
  } else {
    if (!callSizeToFit) {
      [message_ sizeToFit];
      messageFrame = [message_ frame];
    }
    messageFrame.origin.x = (maxWidth - NSWidth(messageFrame)) / 2;
  }
  messageFrame.origin.y =
      titleY - kTitleMessageSpacing - NSHeight(messageFrame);
  [message_ setFrame:messageFrame];

  // Set new frame for help text and link.
  if (help_) {
    if (callSizeToFit)
      [help_.get() sizeToFit];
    CGFloat helpHeight = [help_.get() frame].size.height;
    [help_.get() setFrameSize:NSMakeSize(maxWidth, helpHeight)];
    // Set new frame origin for link.
    NSRect helpFrame = [help_.get() frame];
    CGFloat helpX = (maxWidth - NSWidth(helpFrame)) / 2;
    CGFloat helpY =
        NSMinY(messageFrame) - kMessageLinkSpacing - NSHeight(helpFrame);
    [help_.get() setFrameOrigin:NSMakePoint(helpX, helpY)];
  }
}

- (void)removeHelpText {
  if (help_.get()) {
    [help_.get() removeFromSuperview];
    help_.reset(nil);
  }
}

- (void)initializeHelpText {
  // Replace the help placeholder NSTextField with the real help NSTextView.
  // The former doesn't show links in a nice way, but the latter can't be added
  // in IB without a containing scroll view, so create the NSTextView
  // programmatically. Taken from -[InfoBarController initializeLabel].
  help_.reset(
      [[HyperlinkTextView alloc] initWithFrame:[helpPlaceholder_ frame]]);
  [help_.get() setAutoresizingMask:[helpPlaceholder_ autoresizingMask]];
  [[helpPlaceholder_ superview]
      replaceSubview:helpPlaceholder_ with:help_.get()];
  helpPlaceholder_ = nil;  // Now released.
  [help_.get() setDelegate:self];
  [help_.get() setAlignment:NSCenterTextAlignment];

  // Get the help text and link.
  size_t linkOffset = 0;
  NSString* helpMessage(base::SysUTF16ToNSString(l10n_util::GetStringFUTF16(
      IDS_SAD_TAB_HELP_MESSAGE, string16(), &linkOffset)));
  NSString* helpLink = l10n_util::GetNSString(IDS_SAD_TAB_HELP_LINK);
  NSFont* font = [NSFont systemFontOfSize:[NSFont smallSystemFontSize]];
  [help_.get() setMessageAndLink:helpMessage
                        withLink:helpLink
                        atOffset:linkOffset
                            font:font
                    messageColor:[NSColor whiteColor]
                       linkColor:[NSColor whiteColor]];
}

// Called when someone clicks on the embedded link.
- (BOOL)textView:(NSTextView*)textView
   clickedOnLink:(id)link
         atIndex:(NSUInteger)charIndex {
  if (controller_)
    [controller_ openLearnMoreAboutCrashLink:nil];
  return YES;
}

@end
