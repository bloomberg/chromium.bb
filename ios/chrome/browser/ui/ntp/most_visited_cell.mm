// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/most_visited_cell.h"

#include <memory>

#include "base/mac/bind_objc_block.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/sys_string_conversions.h"
#include "components/favicon/core/fallback_url_util.h"
#import "ios/chrome/browser/ui/ntp/google_landing_data_source.h"
#import "ios/chrome/browser/ui/util/constraints_ui_util.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const CGFloat kFaviconSize = 48;
const CGFloat kImageViewBackgroundColor = 0.941;
const CGFloat kImageViewCornerRadius = 3;
const NSInteger kLabelNumLines = 2;
const CGFloat kLabelTextColor = 0.314;
const CGFloat kMaximumWidth = 73;
const CGFloat kMaximumHeight = 100;

@interface MostVisitedCell () {
  // Backs property with the same name.
  GURL _URL;
  // Weak reference to the relevant GoogleLandingDataSource.
  __weak id<GoogleLandingDataSource> _dataSource;
  // Backs property with the same name.
  ntp_tiles::TileVisualType _tileType;

  UILabel* _label;
  UILabel* _noIconLabel;
  UIImageView* _imageView;
}
// Set the background color and center the first letter of the site title (or
// domain if the title is a url).
- (void)updateIconLabelWithColor:(UIColor*)textColor
                 backgroundColor:(UIColor*)backgroundColor
        isDefaultBackgroundColor:(BOOL)isDefaultBackgroundColor;
// Set icon of top site.
- (void)setImage:(UIImage*)image;
@end

@implementation MostVisitedCell

@synthesize URL = _URL;
@synthesize tileType = _tileType;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (!self) {
    return nil;
  }
  _label = [[UILabel alloc] initWithFrame:CGRectZero];
  [_label setTextColor:[UIColor colorWithWhite:kLabelTextColor alpha:1.0]];
  [_label setBackgroundColor:[UIColor clearColor]];
  [_label setFont:[MDCTypography captionFont]];
  [_label setTextAlignment:NSTextAlignmentCenter];
  CGSize maxSize = [self.class maximumSize];
  [_label setPreferredMaxLayoutWidth:maxSize.width];
  [_label setNumberOfLines:kLabelNumLines];

  _noIconLabel = [[UILabel alloc] initWithFrame:CGRectZero];
  [_noIconLabel setBackgroundColor:[UIColor clearColor]];
  [_noIconLabel setFont:[MDCTypography headlineFont]];
  [_noIconLabel setTextAlignment:NSTextAlignmentCenter];

  _imageView = [[UIImageView alloc] initWithFrame:CGRectZero];
  [_imageView layer].cornerRadius = kImageViewCornerRadius;
  [_imageView setClipsToBounds:YES];
  [self addSubview:_imageView];
  [self addSubview:_label];
  [self addSubview:_noIconLabel];

  [_noIconLabel setTranslatesAutoresizingMaskIntoConstraints:NO];
  [_imageView setTranslatesAutoresizingMaskIntoConstraints:NO];
  [_label setTranslatesAutoresizingMaskIntoConstraints:NO];

  NSDictionary* views =
      @{ @"label" : _label,
         @"image" : _imageView,
         @"noIcon" : _noIconLabel };
  NSArray* constraints = @[
    @"V:|[image(==iconSize)]-10-[label]", @"V:[noIcon(==iconSize)]",
    @"H:[image(==iconSize)]", @"H:|[label]|", @"H:|[noIcon]|"
  ];
  ApplyVisualConstraintsWithMetrics(constraints, views,
                                    @{ @"iconSize" : @(kFaviconSize) });
  AddSameCenterXConstraint(self, _imageView);
  AddSameCenterXConstraint(self, _noIconLabel);

  [self prepareForReuse];
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  [self setImage:nil];
  [self setText:nil];
  // TODO(crbug.com/495600): -stepToVerifyAccessibilityOnCurrentScreen does not
  // honor -setIsAccessibilityElement and setAccessibilityElementsHidden for
  // UICollectionViewCell's, so setting the label to a single space string to
  // pass tests. This string will not be read by screen readers.
  [_label setText:@" "];
  [self setURL:GURL()];
  [_imageView
      setBackgroundColor:[UIColor colorWithWhite:kImageViewBackgroundColor
                                           alpha:1.0]];
  [_noIconLabel setText:nil];
}

#pragma mark - Public Setters

- (void)setText:(NSString*)text {
  [_label setText:text];
  [self setAccessibilityLabel:text];
  [self setIsAccessibilityElement:text != nil];
  [self setAccessibilityElementsHidden:text == nil];
  [self setUserInteractionEnabled:YES];
}

- (void)showPlaceholder {
  [_imageView setBackgroundColor:[[MDCPalette greyPalette] tint50]];
}

- (void)setupWithURL:(GURL)URL
               title:(NSString*)title
          dataSource:(id<GoogleLandingDataSource>)dataSource {
  _dataSource = dataSource;
  _tileType = ntp_tiles::TileVisualType::NONE;
  [self setText:title];
  [self setURL:URL];
  __weak MostVisitedCell* weakSelf = self;

  void (^faviconImageBlock)(UIImage*) = ^(UIImage* favicon) {

    if (URL == [weakSelf URL])  // Tile has not been reused.
      [weakSelf setImage:favicon];
  };

  void (^faviconFallbackBlock)(UIColor*, UIColor*, BOOL) =
      ^(UIColor* textColor, UIColor* backgroundColor, BOOL isDefaultColor) {
        if (URL == [weakSelf URL])  // Tile has not been reused.
          [weakSelf updateIconLabelWithColor:textColor
                             backgroundColor:backgroundColor
                    isDefaultBackgroundColor:isDefaultColor];
      };

  [_dataSource getFaviconForURL:URL
                           size:kFaviconSize
                       useCache:YES
                  imageCallback:faviconImageBlock
               fallbackCallback:faviconFallbackBlock];
}

- (void)removePlaceholderImage {
  [_imageView setBackgroundColor:nil];
  [self setUserInteractionEnabled:NO];
}

#pragma mark - Class methods

+ (CGSize)maximumSize {
  return CGSizeMake(kMaximumWidth, kMaximumHeight);
}

#pragma mark - Private methods

- (void)updateIconLabelWithColor:(UIColor*)textColor
                 backgroundColor:(UIColor*)backgroundColor
        isDefaultBackgroundColor:(BOOL)isDefaultBackgroundColor {
  [self setImage:nil];
  [_noIconLabel
      setText:base::SysUTF16ToNSString(favicon::GetFallbackIconText(_URL))];
  [_noIconLabel setTextColor:textColor];
  [_imageView setBackgroundColor:backgroundColor];
  _tileType = isDefaultBackgroundColor ? ntp_tiles::TileVisualType::ICON_DEFAULT
                                       : ntp_tiles::TileVisualType::ICON_COLOR;
}

- (void)setImage:(UIImage*)image {
  [_imageView setBackgroundColor:nil];
  [_noIconLabel setText:nil];
  [_imageView setImage:image];
  _tileType = ntp_tiles::TileVisualType::ICON_REAL;
}

@end
