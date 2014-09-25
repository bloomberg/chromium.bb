// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/tab_contents/sad_tab_view.h"

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/cocoa/tab_contents/sad_tab_controller.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "grit/theme_resources.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#import "ui/base/cocoa/controls/hyperlink_text_view.h"
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

@interface SadTabTextView : NSTextField

- (id)initWithView:(SadTabView*)view withText:(int)textIds;

@end

@implementation SadTabTextView

- (id)initWithView:(SadTabView*)view withText:(int)textIds {
  if (self = [super init]) {
    [self setTextColor:[NSColor whiteColor]];
    [self setAlignment:NSCenterTextAlignment];
    [self setStringValue:l10n_util::GetNSString(textIds)];
    [self setEditable:NO];
    [self setBezeled:NO];
    [self setAutoresizingMask:
        NSViewMinXMargin|NSViewWidthSizable|NSViewMaxXMargin|NSViewMinYMargin];
    [view addSubview:self];
  }
  return self;
}

- (BOOL)isOpaque {
  return YES;
}

@end

@implementation SadTabView

- (void)awakeFromNib {
  // Load resource for image and set it.
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  NSImage* image = rb.GetNativeImageNamed(IDR_SAD_TAB).ToNSImage();
  [image_ setImage:image];


  // Initialize background color.
  NSColor* backgroundColor = [[NSColor colorWithCalibratedRed:(35.0f/255.0f)
                                                        green:(48.0f/255.0f)
                                                         blue:(64.0f/255.0f)
                                                        alpha:1.0] retain];
  backgroundColor_.reset(backgroundColor);

  // Set up the title.
  title_.reset([[SadTabTextView alloc]
      initWithView:self withText:IDS_SAD_TAB_TITLE]);
  [title_ setFont:[NSFont boldSystemFontOfSize:[NSFont systemFontSize]]];
  [title_ setBackgroundColor:backgroundColor];

  // Set up the message.
  message_.reset([[SadTabTextView alloc]
      initWithView:self withText:IDS_SAD_TAB_MESSAGE]);
  [message_ setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
  [message_ setBackgroundColor:backgroundColor];

  DCHECK(controller_);
  [self initializeHelpText];
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
  CGFloat iconX = floorf((maxWidth - NSWidth(iconFrame)) / 2);
  CGFloat iconY =
      MIN(floorf((NSHeight(newBounds) - NSHeight(iconFrame)) / 2) -
              kSadTabOffset,
          NSHeight(newBounds) - NSHeight(iconFrame));
  iconX = floorf(iconX);
  iconY = floorf(iconY);
  [image_ setFrameOrigin:NSMakePoint(iconX, iconY)];

  // Set new frame origin for title.
  if (callSizeToFit)
    [title_ sizeToFit];
  NSRect titleFrame = [title_ frame];
  CGFloat titleX = floorf((maxWidth - NSWidth(titleFrame)) / 2);
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
    messageFrame.origin.x = floorf((maxWidth - NSWidth(messageFrame)) / 2);
  }
  messageFrame.origin.y =
      titleY - kTitleMessageSpacing - NSHeight(messageFrame);
  [message_ setFrame:messageFrame];

  // Set new frame for help text and link.
  if (help_) {
    if (callSizeToFit)
      [help_ sizeToFit];
    CGFloat helpHeight = [help_ frame].size.height;
    [help_ setFrameSize:NSMakeSize(maxWidth, helpHeight)];
    // Set new frame origin for link.
    NSRect helpFrame = [help_ frame];
    CGFloat helpX = floorf((maxWidth - NSWidth(helpFrame)) / 2);
    CGFloat helpY =
        NSMinY(messageFrame) - kMessageLinkSpacing - NSHeight(helpFrame);
    [help_ setFrameOrigin:NSMakePoint(helpX, helpY)];
  }
}

- (void)removeHelpText {
  if (help_) {
    [help_ removeFromSuperview];
    help_.reset(nil);
  }
}

- (void)initializeHelpText {
  // Programmatically create the help link. Note that the frame's initial
  // height must be set for the programmatic resizing to work.
  help_.reset(
      [[HyperlinkTextView alloc] initWithFrame:NSMakeRect(0, 0, 1, 17)]);
  [help_ setAutoresizingMask:
      NSViewMinXMargin|NSViewWidthSizable|NSViewMaxXMargin|NSViewMinYMargin];
  [self addSubview:help_];
  [help_ setDelegate:self];

  // Get the help text and link.
  size_t linkOffset = 0;
  NSString* helpMessage(base::SysUTF16ToNSString(l10n_util::GetStringFUTF16(
      IDS_SAD_TAB_HELP_MESSAGE, base::string16(), &linkOffset)));
  NSString* helpLink = l10n_util::GetNSString(IDS_SAD_TAB_HELP_LINK);
  NSFont* font = [NSFont systemFontOfSize:[NSFont smallSystemFontSize]];
  [help_ setMessageAndLink:helpMessage
                  withLink:helpLink
                  atOffset:linkOffset
                      font:font
              messageColor:[NSColor whiteColor]
                 linkColor:[NSColor whiteColor]];
  [help_ setAlignment:NSCenterTextAlignment];
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
