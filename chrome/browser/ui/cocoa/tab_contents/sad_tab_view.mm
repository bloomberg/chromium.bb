// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/tab_contents/sad_tab_view.h"

#include "base/logging.h"
#import "chrome/browser/ui/cocoa/hyperlink_button_cell.h"
#include "grit/theme_resources.h"
#import "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "ui/base/resource/resource_bundle.h"

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
  NSImage* image = rb.GetNativeImageNamed(IDR_SAD_TAB);
  DCHECK(image);
  [image_ setImage:image];

  // Set font for title.
  NSFont* titleFont = [NSFont boldSystemFontOfSize:[NSFont systemFontSize]];
  [title_ setFont:titleFont];

  // Set font for message.
  NSFont* messageFont = [NSFont systemFontOfSize:[NSFont smallSystemFontSize]];
  [message_ setFont:messageFont];

  // If necessary, set font and color for link.
  if (linkButton_) {
    [linkButton_ setFont:messageFont];
    [linkCell_ setTextColor:[NSColor whiteColor]];
  }

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

  if (linkButton_) {
    if (callSizeToFit)
      [linkButton_ sizeToFit];
    // Set new frame origin for link.
    NSRect linkFrame = [linkButton_ frame];
    CGFloat linkX = (maxWidth - NSWidth(linkFrame)) / 2;
    CGFloat linkY =
        NSMinY(messageFrame) - kMessageLinkSpacing - NSHeight(linkFrame);
    [linkButton_ setFrameOrigin:NSMakePoint(linkX, linkY)];
  }
}

- (void)removeLinkButton {
  if (linkButton_) {
    [linkButton_ removeFromSuperview];
    linkButton_ = nil;
  }
}

@end
