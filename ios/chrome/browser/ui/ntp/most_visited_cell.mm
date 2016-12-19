// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/most_visited_cell.h"

#include <memory>

#import "base/ios/weak_nsobject.h"
#include "base/mac/bind_objc_block.h"
#import "base/mac/scoped_nsobject.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/favicon/core/fallback_url_util.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/favicon_base/fallback_icon_style.h"
#include "components/favicon_base/favicon_types.h"
#include "components/history/core/browser/top_sites.h"
#include "components/suggestions/suggestions_service.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#include "ios/chrome/browser/favicon/favicon_service_factory.h"
#include "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#include "ios/chrome/browser/favicon/ios_chrome_large_icon_cache_factory.h"
#include "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#include "ios/chrome/browser/favicon/large_icon_cache.h"
#include "ios/chrome/browser/history/top_sites_factory.h"
#include "ios/chrome/browser/suggestions/suggestions_service_factory.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "skia/ext/skia_utils_ios.h"

const CGFloat kFaviconSize = 48;
const CGFloat kFaviconMinSize = 32;
const CGFloat kImageViewBackgroundColor = 0.941;
const CGFloat kImageViewCornerRadius = 3;
const NSInteger kLabelNumLines = 2;
const CGFloat kLabelTextColor = 0.314;
const CGFloat kMaximumWidth = 73;
const CGFloat kMaximumHeight = 100;

@interface MostVisitedCell () {
  // Backs property with the same name.
  GURL _URL;
  // Weak reference to the relevant BrowserState.
  ios::ChromeBrowserState* _browserState;
  // Backs property with the same name.
  ntp_tiles::metrics::MostVisitedTileType _tileType;

  base::scoped_nsobject<UILabel> _label;
  base::scoped_nsobject<UILabel> _noIconLabel;
  base::scoped_nsobject<UIImageView> _imageView;
  // Used to cancel tasks for the LargeIconService.
  base::CancelableTaskTracker _cancelable_task_tracker;
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
@synthesize browserState = _browserState;
@synthesize tileType = _tileType;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (!self) {
    return nil;
  }
  _label.reset([[UILabel alloc] initWithFrame:CGRectZero]);
  [_label setTextColor:[UIColor colorWithWhite:kLabelTextColor alpha:1.0]];
  [_label setBackgroundColor:[UIColor clearColor]];
  [_label setFont:[MDCTypography captionFont]];
  [_label setTextAlignment:NSTextAlignmentCenter];
  CGSize maxSize = [self.class maximumSize];
  [_label setPreferredMaxLayoutWidth:maxSize.width];
  [_label setNumberOfLines:kLabelNumLines];

  _noIconLabel.reset([[UILabel alloc] initWithFrame:CGRectZero]);
  [_noIconLabel setBackgroundColor:[UIColor clearColor]];
  [_noIconLabel setFont:[MDCTypography headlineFont]];
  [_noIconLabel setTextAlignment:NSTextAlignmentCenter];

  _imageView.reset([[UIImageView alloc] initWithFrame:CGRectZero]);
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
        browserState:(ios::ChromeBrowserState*)browserState {
  _browserState = browserState;
  _tileType = ntp_tiles::metrics::NONE;
  [self setText:title];
  [self setURL:URL];
  base::WeakNSObject<MostVisitedCell> weakSelf(self);

  void (^faviconBlock)(const favicon_base::LargeIconResult&) =
      ^(const favicon_base::LargeIconResult& result) {
        base::scoped_nsobject<MostVisitedCell> strongSelf([weakSelf retain]);
        if (!strongSelf)
          return;

        if (URL == [strongSelf URL]) {  // Tile has not been reused.
          if (result.bitmap.is_valid()) {
            scoped_refptr<base::RefCountedMemory> data =
                result.bitmap.bitmap_data.get();
            UIImage* favicon =
                [UIImage imageWithData:[NSData dataWithBytes:data->front()
                                                      length:data->size()]
                                 scale:[UIScreen mainScreen].scale];
            [strongSelf setImage:favicon];
          } else if (result.fallback_icon_style) {
            UIColor* backgroundColor = skia::UIColorFromSkColor(
                result.fallback_icon_style->background_color);
            UIColor* textColor = skia::UIColorFromSkColor(
                result.fallback_icon_style->text_color);
            [strongSelf updateIconLabelWithColor:textColor
                                 backgroundColor:backgroundColor
                        isDefaultBackgroundColor:result.fallback_icon_style->
                                                 is_default_background_color];
          }
        }

        if (result.bitmap.is_valid() || result.fallback_icon_style) {
          IOSChromeLargeIconCacheFactory::GetForBrowserState(
              [strongSelf browserState])
              ->SetCachedResult(URL, result);
        }
      };

  LargeIconCache* cache =
      IOSChromeLargeIconCacheFactory::GetForBrowserState(self.browserState);
  std::unique_ptr<favicon_base::LargeIconResult> cached_result =
      cache->GetCachedResult(URL);
  if (cached_result) {
    faviconBlock(*cached_result);
  }

  // Always call LargeIconService in case the favicon was updated.
  favicon::LargeIconService* large_icon_service =
      IOSChromeLargeIconServiceFactory::GetForBrowserState(self.browserState);
  CGFloat faviconSize = [UIScreen mainScreen].scale * kFaviconSize;
  CGFloat faviconMinSize = [UIScreen mainScreen].scale * kFaviconMinSize;
  large_icon_service->GetLargeIconOrFallbackStyle(
      URL, faviconMinSize, faviconSize, base::BindBlock(faviconBlock),
      &_cancelable_task_tracker);
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
  _tileType = isDefaultBackgroundColor ? ntp_tiles::metrics::ICON_DEFAULT
                                       : ntp_tiles::metrics::ICON_COLOR;
}

- (void)setImage:(UIImage*)image {
  [_imageView setBackgroundColor:nil];
  [_noIconLabel setText:nil];
  [_imageView setImage:image];
  _tileType = ntp_tiles::metrics::ICON_REAL;
}

@end
