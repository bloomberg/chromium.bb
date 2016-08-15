// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/tab_contents/sad_tab_view_cocoa.h"

#import "base/mac/foundation_util.h"
#include "components/grit/components_scaled_resources.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#import "ui/base/cocoa/controls/blue_label_button.h"
#import "ui/base/cocoa/nscolor_additions.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

// Maximum width used by page contents.
const CGFloat kMaxContainerWidth = 600;
// Padding between icon and title.
const CGFloat kIconTitleSpacing = 40;
// Padding between title and message.
const CGFloat kTitleMessageSpacing = 18;
// Padding between message and link.
const CGFloat kMessageLinkSpacing = 50;
// Padding between message and button.
const CGFloat kMessageButtonSpacing = 44;
// Minimum margins on all sides.
const CGFloat kTabMargin = 13;
// Maximum margin on top.
const CGFloat kMaxTopMargin = 130;

NSTextField* MakeLabelTextField(NSRect frame) {
  NSTextField* ret = [[[NSTextField alloc] initWithFrame:frame] autorelease];
  ret.autoresizingMask = NSViewWidthSizable | NSViewMaxYMargin;
  ret.editable = NO;
  ret.drawsBackground = NO;
  ret.bezeled = NO;
  return ret;
}

}  // namespace

@interface SadTabContainerView : NSView
@end

@implementation SadTabContainerView
- (BOOL)isFlipped {
  return YES;
}
@end

@interface SadTabView ()<NSTextViewDelegate>
@end

@implementation SadTabView {
  NSView* container_;
  NSImageView* icon_;
  NSTextField* title_;
  NSTextField* message_;
  HyperlinkTextView* help_;
  NSButton* button_;
}

@synthesize delegate = delegate_;

- (instancetype)initWithFrame:(NSRect)frame {
  if ((self = [super initWithFrame:frame])) {
    self.wantsLayer = YES;
    self.layer.backgroundColor =
        [NSColor colorWithCalibratedWhite:245.0f / 255.0f alpha:1.0].cr_CGColor;
    container_ = [[SadTabContainerView new] autorelease];

    NSImage* iconImage = ResourceBundle::GetSharedInstance()
                             .GetNativeImageNamed(IDR_CRASH_SAD_TAB)
                             .ToNSImage();
    icon_ = [[NSImageView new] autorelease];
    icon_.image = iconImage;
    icon_.frameSize = iconImage.size;
    icon_.autoresizingMask = NSViewMaxXMargin | NSViewMaxYMargin;
    [container_ addSubview:icon_];

    CGFloat width = self.bounds.size.width;
    title_ = MakeLabelTextField(
        NSMakeRect(0, NSMaxY(icon_.frame) + kIconTitleSpacing, width, 0));
    title_.font = [NSFont systemFontOfSize:24];
    title_.textColor =
        [NSColor colorWithCalibratedWhite:38.0f / 255.0f alpha:1.0];
    [title_ sizeToFit];
    [container_ addSubview:title_];

    message_ =
        MakeLabelTextField(NSMakeRect(0, NSMaxY(title_.frame), width, 0));
    base::mac::ObjCCast<NSCell>(message_.cell).wraps = YES;
    message_.font = [NSFont systemFontOfSize:14];
    message_.textColor =
        [NSColor colorWithCalibratedWhite:81.0f / 255.0f alpha:1.0];
    [container_ addSubview:message_];

    help_ = [[[HyperlinkTextView alloc]
        initWithFrame:NSMakeRect(0, 0, 1, message_.font.pointSize + 4)]
        autorelease];
    help_.delegate = self;
    help_.autoresizingMask = NSViewWidthSizable | NSViewMaxYMargin;
    help_.textContainer.lineFragmentPadding = 2;  // To align with message_.
    [container_ addSubview:help_];

    button_ = [[BlueLabelButton new] autorelease];
    button_.target = self;
    button_.action = @selector(buttonClicked);
    [container_ addSubview:button_];

    [self addSubview:container_];
  }
  return self;
}

- (BOOL)isFlipped {
  return YES;
}

- (void)resizeSubviewsWithOldSize:(NSSize)oldSize {
  [super resizeSubviewsWithOldSize:oldSize];

  NSSize size = self.bounds.size;
  NSSize containerSize = NSMakeSize(
      std::min(size.width - 2 * kTabMargin, kMaxContainerWidth), size.height);

  // Set the container's size first because text wrapping depends on its width.
  container_.frameSize = containerSize;

  // |message_| can wrap to variable number of lines.
  [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:message_];

  message_.frameOrigin =
      NSMakePoint(0, NSMaxY(title_.frame) + kTitleMessageSpacing);
  help_.frameOrigin =
      NSMakePoint(0, NSMaxY(message_.frame) + kMessageLinkSpacing);

  button_.frameOrigin =
      NSMakePoint(containerSize.width - NSWidth(button_.bounds),
                  NSMaxY(message_.frame) + kMessageButtonSpacing);

  containerSize.height = NSMaxY(button_.frame);
  container_.frameSize = containerSize;

  // Center. Top margin is must be between kTabMargin and kMaxTopMargin.
  container_.frameOrigin = NSMakePoint(
      floor((size.width - containerSize.width) / 2),
      std::min(kMaxTopMargin,
               std::max(kTabMargin,
                        size.height - containerSize.height - kTabMargin)));
}

- (void)setTitle:(int)title {
  NSMutableParagraphStyle* style = [[NSMutableParagraphStyle new] autorelease];
  style.lineSpacing = 6;

  title_.attributedStringValue = [[[NSAttributedString alloc]
      initWithString:l10n_util::GetNSString(title)
          attributes:@{NSParagraphStyleAttributeName : style}] autorelease];
}

- (void)setMessage:(int)message {
  message_.stringValue = l10n_util::GetNSString(message);
}

- (void)setButtonTitle:(int)buttonTitle {
  button_.title = l10n_util::GetNSString(buttonTitle);
  [button_ sizeToFit];
}

- (void)setHelpLinkTitle:(int)helpLinkTitle URL:(NSString*)url {
  NSString* title = l10n_util::GetNSString(helpLinkTitle);
  [help_ setMessage:title
           withFont:message_.font
       messageColor:message_.textColor];
  [help_ addLinkRange:NSMakeRange(0, title.length)
              withURL:url
            linkColor:message_.textColor];
  [help_ sizeToFit];
}

- (void)buttonClicked {
  [delegate_ sadTabViewButtonClicked:self];
}

// Called when someone clicks on the embedded link.
- (BOOL)textView:(NSTextView*)textView
    clickedOnLink:(id)link
          atIndex:(NSUInteger)charIndex {
  [delegate_ sadTabView:self helpLinkClickedWithURL:(NSString*)link];
  return YES;
}

@end
