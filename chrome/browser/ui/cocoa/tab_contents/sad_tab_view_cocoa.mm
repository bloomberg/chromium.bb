// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/tab_contents/sad_tab_view_cocoa.h"

#import "base/mac/foundation_util.h"
#include "components/grit/components_scaled_resources.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#import "ui/base/cocoa/controls/blue_label_button.h"
#import "ui/base/cocoa/controls/hyperlink_text_view.h"
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
  // Not a scoped_nsobject for easy property access.
  NSTextField* ret = [[[NSTextField alloc] initWithFrame:frame] autorelease];
  ret.autoresizingMask = NSViewWidthSizable;
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
  NSTextField* message_;
  HyperlinkTextView* help_;
  NSButton* button_;
  chrome::SadTab* sadTab_;
}

- (instancetype)initWithFrame:(NSRect)frame sadTab:(chrome::SadTab*)sadTab {
  if ((self = [super initWithFrame:frame])) {
    sadTab_ = sadTab;

    self.wantsLayer = YES;
    self.layer.backgroundColor =
        [NSColor colorWithCalibratedWhite:245.0f / 255.0f alpha:1.0].CGColor;
    container_ = [[SadTabContainerView new] autorelease];

    NSImage* iconImage = ResourceBundle::GetSharedInstance()
                             .GetNativeImageNamed(IDR_CRASH_SAD_TAB)
                             .ToNSImage();
    NSImageView* icon = [[NSImageView new] autorelease];
    icon.image = iconImage;
    icon.frameSize = iconImage.size;
    [container_ addSubview:icon];

    NSTextField* title = MakeLabelTextField(
        NSMakeRect(0, NSMaxY(icon.frame) + kIconTitleSpacing, 0, 0));
    title.font = [NSFont systemFontOfSize:24];
    title.textColor =
        [NSColor colorWithCalibratedWhite:38.0f / 255.0f alpha:1.0];

    NSMutableParagraphStyle* titleStyle =
        [[NSMutableParagraphStyle new] autorelease];
    titleStyle.lineSpacing = 6;
    title.attributedStringValue = [[[NSAttributedString alloc]
        initWithString:l10n_util::GetNSString(sadTab->GetTitle())
            attributes:@{NSParagraphStyleAttributeName : titleStyle}]
        autorelease];

    [title sizeToFit];
    [container_ addSubview:title];

    NSFont* messageFont = [NSFont systemFontOfSize:14];
    NSColor* messageColor =
        [NSColor colorWithCalibratedWhite:81.0f / 255.0f alpha:1.0];

    message_ = MakeLabelTextField(NSMakeRect(0, NSMaxY(title.frame), 0, 0));
    message_.frameOrigin =
        NSMakePoint(0, NSMaxY(title.frame) + kTitleMessageSpacing);
    base::mac::ObjCCast<NSCell>(message_.cell).wraps = YES;
    message_.font = messageFont;
    message_.textColor = messageColor;
    message_.stringValue = l10n_util::GetNSString(sadTab->GetMessage());
    [container_ addSubview:message_];

    NSString* helpLinkTitle =
        l10n_util::GetNSString(sadTab->GetHelpLinkTitle());
    help_ = [[[HyperlinkTextView alloc]
        initWithFrame:NSMakeRect(0, 0, 1, message_.font.pointSize + 4)]
        autorelease];
    help_.delegate = self;
    help_.autoresizingMask = NSViewWidthSizable;
    help_.textContainer.lineFragmentPadding = 2;  // To align with message_.
    [help_ setMessage:helpLinkTitle
             withFont:messageFont
         messageColor:messageColor];
    [help_ addLinkRange:NSMakeRange(0, helpLinkTitle.length)
                withURL:@(sadTab->GetHelpLinkURL())
              linkColor:messageColor];
    [help_ sizeToFit];
    [container_ addSubview:help_];

    button_ = [[BlueLabelButton new] autorelease];
    button_.target = self;
    button_.action = @selector(buttonClicked);
    button_.title = l10n_util::GetNSString(sadTab->GetButtonTitle());
    [button_ sizeToFit];
    [container_ addSubview:button_];

    [self addSubview:container_];
    [self resizeSubviewsWithOldSize:self.bounds.size];
  }
  return self;
}

- (BOOL)isOpaque {
  return YES;
}

- (BOOL)isFlipped {
  return YES;
}

- (void)updateLayer {
  // Currently, updateLayer is only called once. If that changes, a DCHECK in
  // SadTab::RecordFirstPaint will pipe up and we should add a guard here.
  sadTab_->RecordFirstPaint();
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

- (void)buttonClicked {
  sadTab_->PerformAction(chrome::SadTab::Action::BUTTON);
}

// Called when someone clicks on the embedded link.
- (BOOL)textView:(NSTextView*)textView
    clickedOnLink:(id)link
          atIndex:(NSUInteger)charIndex {
  sadTab_->PerformAction(chrome::SadTab::Action::HELP_LINK);
  return YES;
}

@end
