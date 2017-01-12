// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/history/tab_history_cell.h"

#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/third_party/material_roboto_font_loader_ios/src/src/MaterialRobotoFontLoader.h"
#import "ios/web/navigation/crw_session_entry.h"
#include "ios/web/public/navigation_item.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Header horizontal layout inset.
const CGFloat kHeaderHorizontalInset = 16;
// Header vertical layout inset.
const CGFloat kHeaderVerticalInset = 16;
// Width and Height for the site icon view.
const CGFloat kSiteIconViewWidth = 16;
// Height adjustment for the line view. The design calls for some overlap.
const CGFloat kHeaderHeightAdjustment = 6;
// Margin between the icon and the line view in the header.
const CGFloat kLineViewTopMargin = 3;
// Color for the line with in the header section.
const int kHeaderLineRGB = 0xE5E5E5;
// Color for the footer line.
const int kFooterRGB = 0xD2D2D2;
// Color for the text in the title label.
const int kTitleTextRGB = 0x333333;
// Width for the header line view.
NS_INLINE CGFloat HeaderLineWidth() {
  return IsHighResScreen() ? 1.5 : 2;
}
// Corner radius for the header line view.
NS_INLINE CGFloat HeaderLineRadius() {
  return HeaderLineWidth() / 2.0;
}
}

@implementation TabHistoryCell {
  CRWSessionEntry* _entry;
  UILabel* _titleLabel;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self)
    [self commonInitialization];

  return self;
}

- (instancetype)initWithCoder:(NSCoder*)coder {
  self = [super initWithCoder:coder];
  if (self)
    [self commonInitialization];

  return self;
}

- (void)commonInitialization {
  _titleLabel = [[UILabel alloc] initWithFrame:CGRectZero];
  [_titleLabel setTextColor:UIColorFromRGB(kTitleTextRGB)];
  [_titleLabel
      setFont:[[MDFRobotoFontLoader sharedInstance] regularFontOfSize:16]];
  [[self contentView] addSubview:_titleLabel];
}

- (void)layoutSubviews {
  [super layoutSubviews];

  CGRect bounds = [[self contentView] bounds];
  CGRect frame = AlignRectOriginAndSizeToPixels(bounds);
  [_titleLabel setFrame:frame];
}

- (CRWSessionEntry*)entry {
  return _entry;
}

- (void)setEntry:(CRWSessionEntry*)entry {
  _entry = entry;

  NSString* title = nil;
  web::NavigationItem* navigationItem = [_entry navigationItem];
  if (navigationItem) {
    // TODO(rohitrao): Can this use GetTitleForDisplay() instead?
    if (navigationItem->GetTitle().empty())
      title = base::SysUTF8ToNSString(navigationItem->GetURL().spec());
    else
      title = base::SysUTF16ToNSString(navigationItem->GetTitle());
  }

  [_titleLabel setText:title];
  [self setAccessibilityLabel:title];
  [self setNeedsLayout];
}

- (UILabel*)titleLabel {
  return _titleLabel;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  _entry = nil;
  [_titleLabel setText:nil];
  [self setAccessibilityLabel:nil];
}

@end

@implementation TabHistorySectionHeader {
  UIImageView* _iconView;
  UIView* _lineView;
}

- (UIImageView*)iconView {
  return _iconView;
}

- (UIView*)lineView {
  return _lineView;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    CGRect iconFrame = CGRectMake(0, 0, kSiteIconViewWidth, kSiteIconViewWidth);
    _iconView = [[UIImageView alloc] initWithFrame:iconFrame];
    [self addSubview:_iconView];

    UIColor* lineColor = UIColorFromRGB(kHeaderLineRGB);

    _lineView = [[UIView alloc] initWithFrame:CGRectZero];
    [[_lineView layer] setCornerRadius:HeaderLineRadius()];
    [_lineView setBackgroundColor:lineColor];
    [self addSubview:_lineView];
  }

  return self;
}

- (void)layoutSubviews {
  [super layoutSubviews];

  CGRect bounds =
      CGRectInset([self bounds], kHeaderHorizontalInset, kHeaderVerticalInset);

  CGRect iconViewFrame = AlignRectToPixel(bounds);
  iconViewFrame.size = CGSizeMake(kSiteIconViewWidth, kSiteIconViewWidth);
  [_iconView setFrame:iconViewFrame];

  CGFloat iconViewMaxY = CGRectGetMaxY(iconViewFrame);
  CGFloat height =
      CGRectGetHeight(bounds) - iconViewMaxY + kHeaderHeightAdjustment;
  if (height < 0)
    height = 0;

  CGRect lineViewFrame = CGRectZero;
  lineViewFrame.origin.x = CGRectGetMidX(bounds) - HeaderLineWidth() / 2.0;
  lineViewFrame.origin.y = iconViewMaxY + kLineViewTopMargin;
  lineViewFrame.size.width = HeaderLineWidth();
  lineViewFrame.size.height = height;
  lineViewFrame = AlignRectOriginAndSizeToPixels(lineViewFrame);

  [_lineView setFrame:lineViewFrame];
}

@end

@implementation TabHistorySectionFooter

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self)
    [self setBackgroundColor:UIColorFromRGB(kFooterRGB)];

  return self;
}

@end
