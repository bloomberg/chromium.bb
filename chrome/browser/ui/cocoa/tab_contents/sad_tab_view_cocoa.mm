// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/tab_contents/sad_tab_view_cocoa.h"

#include <stddef.h>

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/grit/components_scaled_resources.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#import "ui/base/cocoa/controls/blue_label_button.h"
#import "ui/base/cocoa/controls/hyperlink_text_view.h"
#import "ui/base/cocoa/nscolor_additions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

// Maximum width used by page contents.
static const CGFloat kMaxContainerWidth = 600;
// Padding between icon and title.
static const CGFloat kIconTitleSpacing = 40;
// Padding between title and message.
static const CGFloat kTitleMessageSpacing = 18;
// Padding between message and link.
static const CGFloat kMessageLinkSpacing = 50;
// Padding between message and button.
static const CGFloat kMessageButtonSpacing = 44;
// Minimum margins on all sides.
static const CGFloat kTabMargin = 13;
// Maximum margin on top.
static const CGFloat kMaxTopMargin = 130;

@interface SadTabTextView : NSTextField

- (id)initWithStringResourceID:(int)stringResourceID;

@end

@implementation SadTabTextView

- (id)initWithStringResourceID:(int)stringResourceID {
  if (self = [super init]) {
    base::scoped_nsobject<NSMutableParagraphStyle> style(
        [[NSMutableParagraphStyle alloc] init]);
    [style setLineSpacing:6];
    base::scoped_nsobject<NSAttributedString> title([[NSAttributedString alloc]
        initWithString:l10n_util::GetNSString(stringResourceID)
            attributes:@{ NSParagraphStyleAttributeName : style }]);
    [self setAttributedStringValue:title];

    [self setAlignment:NSLeftTextAlignment];
    [self setEditable:NO];
    [self setBezeled:NO];
    [self setAutoresizingMask:NSViewWidthSizable|NSViewMaxYMargin];
  }
  return self;
}

- (BOOL)isOpaque {
  return YES;
}

@end

@interface SadTabContainerView : NSView<NSTextViewDelegate> {
 @private
  base::scoped_nsobject<NSImageView> image_;
  base::scoped_nsobject<NSTextField> title_;
  base::scoped_nsobject<NSTextField> message_;
  base::scoped_nsobject<HyperlinkTextView> help_;
  base::scoped_nsobject<NSButton> button_;
}

- (instancetype)initWithBackgroundColor:(NSColor*)backgroundColor;

@property(readonly,nonatomic) NSButton* reloadButton;

// The height to fit the content elements within the current width.
@property(readonly,nonatomic) CGFloat contentHeight;

@end

@implementation SadTabContainerView

- (instancetype)initWithBackgroundColor:(NSColor*)backgroundColor {
  if ((self = [super initWithFrame:NSZeroRect])) {
    // Load resource for image and set it.
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    NSImage* iconImage = rb.GetNativeImageNamed(IDR_CRASH_SAD_TAB).ToNSImage();
    NSRect imageFrame = NSZeroRect;
    imageFrame.size = [iconImage size];
    image_.reset([[NSImageView alloc] initWithFrame:imageFrame]);
    [image_ setImage:iconImage];
    [image_ setAutoresizingMask:NSViewMaxXMargin|NSViewMaxYMargin];
    [self addSubview:image_];

    // Set up the title.
    title_.reset(
        [[SadTabTextView alloc] initWithStringResourceID:IDS_SAD_TAB_TITLE]);
    [title_ setFont:[NSFont systemFontOfSize:24]];
    [title_ setBackgroundColor:backgroundColor];
    [title_ setTextColor:[NSColor colorWithCalibratedWhite:38.0f/255.0f
                                                     alpha:1.0]];
    [title_ sizeToFit];
    [title_ setFrameOrigin:
        NSMakePoint(0, NSMaxY(imageFrame) + kIconTitleSpacing)];
    [self addSubview:title_];

    // Set up the message.
    message_.reset(
        [[SadTabTextView alloc] initWithStringResourceID:IDS_SAD_TAB_MESSAGE]);
    [message_ setFont:[NSFont systemFontOfSize:14]];
    [message_ setBackgroundColor:backgroundColor];
    [message_ setTextColor:[NSColor colorWithCalibratedWhite:81.0f/255.0f
                                                       alpha:1.0]];
    [message_ setFrameOrigin:
        NSMakePoint(0, NSMaxY([title_ frame]) + kTitleMessageSpacing)];
    [self addSubview:message_];

    [self initializeHelpText];

    button_.reset([[BlueLabelButton alloc] init]);
    [button_ setTitle:l10n_util::GetNSString(IDS_SAD_TAB_RELOAD_LABEL)];
    [button_ sizeToFit];
    [button_ setTarget:self];
    [button_ setAction:@selector(reloadPage:)];
    [self addSubview:button_];
  }
  return self;
}

- (BOOL)isFlipped {
  return YES;
}

- (NSButton*)reloadButton {
  return button_;
}

- (CGFloat)contentHeight {
  return NSMaxY([button_ frame]);
}

- (void)resizeSubviewsWithOldSize:(NSSize)oldSize {
  [super resizeSubviewsWithOldSize:oldSize];

  // |message_| can wrap to variable number of lines.
  [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:message_];

  [help_ setFrameOrigin:
      NSMakePoint(0, NSMaxY([message_ frame]) + kMessageLinkSpacing)];

  [button_ setFrameOrigin:
      NSMakePoint(NSMaxX([self bounds]) - NSWidth([button_ frame]),
                  NSMaxY([message_ frame]) + kMessageButtonSpacing)];
}

- (void)initializeHelpText {
  // Programmatically create the help link. Note that the frame's initial
  // height must be set for the programmatic resizing to work.
  NSFont* helpFont = [message_ font];
  NSRect helpFrame = NSMakeRect(0, 0, 1, [helpFont pointSize] + 4);
  help_.reset([[HyperlinkTextView alloc] initWithFrame:helpFrame]);
  [help_ setAutoresizingMask:NSViewWidthSizable|NSViewMaxYMargin];
  [help_ setDrawsBackground:YES];
  [help_ setBackgroundColor:[message_ backgroundColor]];
  [[help_ textContainer] setLineFragmentPadding:2];  // To align with message_.
  [self addSubview:help_];
  [help_ setDelegate:self];

  // Get the help text and link.
  size_t linkOffset = 0;
  const base::string16 helpLink =
      l10n_util::GetStringUTF16(IDS_SAD_TAB_LEARN_MORE_LINK);
  NSString* helpMessage(base::SysUTF16ToNSString(helpLink));
  [help_ setMessage:helpMessage
           withFont:helpFont
       messageColor:[message_ textColor]];
  [help_ addLinkRange:NSMakeRange(linkOffset, helpLink.length())
              withURL:@(chrome::kCrashReasonURL)
            linkColor:[message_ textColor]];
  [help_ setAlignment:NSLeftTextAlignment];
  [help_ sizeToFit];
}

// Called when someone clicks on the embedded link.
- (BOOL)textView:(NSTextView*)textView
   clickedOnLink:(id)link
         atIndex:(NSUInteger)charIndex {
  [NSApp sendAction:@selector(openLearnMoreAboutCrashLink:) to:nil from:self];
  return YES;
}

@end

@implementation SadTabView

+ (NSColor*)backgroundColor {
  return [NSColor colorWithCalibratedWhite:245.0f/255.0f alpha:1.0];
}

- (instancetype)initWithFrame:(NSRect)frame {
  if ((self = [super initWithFrame:frame])) {
    [self setWantsLayer:YES];

    container_.reset([[SadTabContainerView alloc]
        initWithBackgroundColor:[SadTabView backgroundColor]]);
    [self addSubview:container_];
  }
  return self;
}

- (CALayer*)makeBackingLayer {
  CALayer* layer = [super makeBackingLayer];
  [layer setBackgroundColor:[[SadTabView backgroundColor] cr_CGColor]];
  return layer;
}

- (BOOL)isFlipped {
  return YES;
}

- (void)resizeSubviewsWithOldSize:(NSSize)oldSize {
  NSRect bounds = [self bounds];

  // Set the container size first because its contentHeight will depend on its
  // width.
  NSSize frameSize = NSMakeSize(
      std::min(NSWidth(bounds) - 2 * kTabMargin, kMaxContainerWidth),
      NSHeight(bounds));
  [container_ setFrameSize:frameSize];

  // Center horizontally.
  // Top margin is at least kTabMargin and at most kMaxTopMargin.
  [container_ setFrameOrigin:NSMakePoint(
      floor((NSWidth(bounds) - frameSize.width) / 2),
      std::min(kMaxTopMargin, std::max(kTabMargin,
          NSHeight(bounds) - [container_ contentHeight] - kTabMargin)))];
}

- (NSButton*)reloadButton {
  return [container_ reloadButton];
}

@end
