// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/today_extension/notification_center_url_button.h"

#import "base/mac/foundation_util.h"
#import "base/mac/scoped_block.h"
#import "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/common/physical_web/physical_web_device.h"
#import "ios/chrome/today_extension/ui_util.h"
#include "net/base/escape.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
const CGFloat kSubtitleFontSize = 14;
}

@implementation NotificationCenterURLButton {
  base::mac::ScopedBlock<URLActionBlock> _openURLBlock;
  base::scoped_nsobject<NSString> _url;
  base::scoped_nsobject<UILabel> _subtitleLabel;
  base::scoped_nsobject<UIView> _bottomSeparator;
}

- (void)openURL:(id)sender {
  _openURLBlock.get()(_url);
}

- (NSString*)unescapeURLString:(NSString*)urlString {
  std::string escapedURL = base::SysNSStringToUTF8(urlString);
  std::string unescapedURL =
      net::UnescapeURLComponent(escapedURL, net::UnescapeRule::SPACES);
  NSString* unescapedURLString = base::SysUTF8ToNSString(unescapedURL);
  return unescapedURLString;
}

// Create a button with contextual URL layout.
- (instancetype)initWithTitle:(NSString*)title
                          url:(NSString*)url
                         icon:(NSString*)icon
                    leftInset:(CGFloat)leftInset
                        block:(URLActionBlock)block {
  self = [super initWithTitle:title
                         icon:nil
                       target:nil
                       action:@selector(openURL:)
              backgroundColor:[UIColor clearColor]
                     inkColor:ui_util::InkColor()
                   titleColor:ui_util::TitleColor()];
  if (self) {
    _openURLBlock.reset(block, base::scoped_policy::RETAIN);
    [self setContentVerticalAlignment:UIControlContentVerticalAlignmentTop];

    _url.reset([url copy]);

    UIImage* iconImage = [[UIImage imageNamed:icon]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    [self setImage:iconImage forState:UIControlStateNormal];
    [[self imageView] setTintColor:ui_util::NormalTintColor()];

    CGFloat chromeIconXOffset = leftInset + ui_util::ChromeIconOffset();

    _subtitleLabel.reset([[UILabel alloc] initWithFrame:CGRectZero]);
    [self setTranslatesAutoresizingMaskIntoConstraints:NO];
    [_subtitleLabel setTranslatesAutoresizingMaskIntoConstraints:NO];
    [_subtitleLabel setText:url];
    [self addSubview:_subtitleLabel];

    [self setTitle:title url:url];

    UIFont* subtitleFont =
        [UIFont fontWithName:@"Helvetica" size:kSubtitleFontSize];
    [_subtitleLabel setFont:subtitleFont];
    [_subtitleLabel setTextColor:ui_util::TextColor()];

    _bottomSeparator.reset([[UIView alloc] initWithFrame:CGRectZero]);
    [_bottomSeparator setBackgroundColor:ui_util::BackgroundColor()];
    [_bottomSeparator setHidden:YES];
    [[self contentView] addSubview:_bottomSeparator];

    [self setContentEdgeInsets:UIEdgeInsetsMake(
                                   ui_util::kSecondLineVerticalPadding,
                                   chromeIconXOffset, 0, chromeIconXOffset)];
    if ([UIView userInterfaceLayoutDirectionForSemanticContentAttribute:
                    self.semanticContentAttribute] ==
        UIUserInterfaceLayoutDirectionRightToLeft) {
      // RTL
      [[self titleLabel] setTextAlignment:NSTextAlignmentNatural];
      [_subtitleLabel setTextAlignment:NSTextAlignmentNatural];
      [self setTitleEdgeInsets:UIEdgeInsetsMake(0, 0, 0,
                                                ui_util::ChromeTextOffset())];
      [self setContentHorizontalAlignment:
                UIControlContentHorizontalAlignmentRight];
    } else {
      // LTR
      [[self titleLabel] setTextAlignment:NSTextAlignmentNatural];
      [_subtitleLabel setTextAlignment:NSTextAlignmentNatural];
      [self setTitleEdgeInsets:UIEdgeInsetsMake(0, ui_util::ChromeTextOffset(),
                                                0, 0)];
      [self setContentHorizontalAlignment:
                UIControlContentHorizontalAlignmentLeft];
    }

    [self setIsAccessibilityElement:YES];
    [self setAccessibilityTraits:UIAccessibilityTraitLink];
  }

  return self;
}

- (void)layoutSubviews {
  [super layoutSubviews];
  CGRect frame = [_subtitleLabel frame];
  CGRect titleFrame = [[self titleLabel] frame];
  CGRect separatorFrame = CGRectZero;

  frame.origin.y = CGRectGetMaxY(titleFrame);
  frame.size.height = 18;

  separatorFrame.size.height = 1.0 / [[UIScreen mainScreen] scale];
  separatorFrame.origin.y =
      [self frame].size.height - separatorFrame.size.height;

  if ([UIView userInterfaceLayoutDirectionForSemanticContentAttribute:
                  self.semanticContentAttribute] ==
      UIUserInterfaceLayoutDirectionRightToLeft) {
    // RTL
    // Align on right
    frame.origin.x = 10;
    frame.size.width = CGRectGetMaxX(titleFrame) - frame.origin.x;
    separatorFrame.origin.x = 0;
    separatorFrame.size.width = CGRectGetMaxX([self imageView].frame);
  } else {
    // LTR
    // Align on left
    frame.origin.x = titleFrame.origin.x;
    frame.size.width = self.frame.size.width - frame.origin.x - 10;
    separatorFrame.origin.x = [self imageView].frame.origin.x;
    separatorFrame.size.width = self.frame.size.width - separatorFrame.origin.x;
  }

  [_subtitleLabel setFrame:frame];
  [_bottomSeparator setFrame:separatorFrame];
}

- (void)setTitle:(NSString*)title url:(NSString*)url {
  _url.reset([url copy]);
  NSString* subtitle = nil;
  if (![title length]) {
    title = [self unescapeURLString:url];
  } else {
    subtitle = [self unescapeURLString:url];
  }
  [super setTitle:title forState:UIControlStateNormal];
  if (subtitle) {
    [_subtitleLabel setText:subtitle];
  } else {
    [_subtitleLabel setText:@""];
  }
}

- (void)setHighlighted:(BOOL)highlighted {
  [super setHighlighted:highlighted];
  if (highlighted) {
    [_subtitleLabel setTextColor:[UIColor colorWithWhite:1 alpha:1]];
    [[self imageView] setTintColor:ui_util::ActiveTintColor()];
  } else {
    [_subtitleLabel setTextColor:ui_util::TextColor()];
    [[self imageView] setTintColor:ui_util::NormalTintColor()];
  }
}

- (void)setSeparatorVisible:(BOOL)visible {
  [_bottomSeparator setHidden:!visible];
}

@end
