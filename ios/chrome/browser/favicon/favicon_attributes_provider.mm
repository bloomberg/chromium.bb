// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/favicon/favicon_attributes_provider.h"

#include "base/logging.h"
#include "base/mac/bind_objc_block.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/favicon/core/fallback_url_util.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/favicon_base/fallback_icon_style.h"
#include "components/favicon_base/favicon_types.h"
#include "skia/ext/skia_utils_ios.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FaviconAttributesProvider () {
  // Used to cancel tasks for the LargeIconService.
  base::CancelableTaskTracker _faviconTaskTracker;
}

@end

@implementation FaviconAttributesProvider
@synthesize largeIconService = _largeIconService;
@synthesize minSize = _minSize;
@synthesize faviconSize = _faviconSize;

- (instancetype)initWithFaviconSize:(CGFloat)faviconSize
                     minFaviconSize:(CGFloat)minFaviconSize
                   largeIconService:
                       (favicon::LargeIconService*)largeIconService {
  DCHECK(largeIconService);
  self = [super init];
  if (self) {
    _faviconSize = faviconSize;
    _minSize = minFaviconSize;
    _largeIconService = largeIconService;
  }

  return self;
}

- (void)fetchFaviconAttributesForURL:(const GURL&)URL
                          completion:(void (^)(FaviconAttributes*))completion {
  GURL blockURL(URL);
  void (^faviconBlock)(const favicon_base::LargeIconResult&) =
      ^(const favicon_base::LargeIconResult& result) {
        FaviconAttributes* attributes = nil;
        if (result.bitmap.is_valid()) {
          scoped_refptr<base::RefCountedMemory> data =
              result.bitmap.bitmap_data.get();
          UIImage* favicon =
              [UIImage imageWithData:[NSData dataWithBytes:data->front()
                                                    length:data->size()]];
          attributes = [FaviconAttributes attributesWithImage:favicon];
        } else if (result.fallback_icon_style) {
          UIColor* backgroundColor = skia::UIColorFromSkColor(
              result.fallback_icon_style->background_color);
          UIColor* textColor =
              skia::UIColorFromSkColor(result.fallback_icon_style->text_color);
          NSString* monogram =
              base::SysUTF16ToNSString(favicon::GetFallbackIconText(blockURL));
          attributes =
              [FaviconAttributes attributesWithMonogram:monogram
                                              textColor:textColor
                                        backgroundColor:backgroundColor];
        }
        DCHECK(attributes);
        completion(attributes);
      };

  // Always call LargeIconService in case the favicon was updated.
  CGFloat faviconSize = [UIScreen mainScreen].scale * self.faviconSize;
  CGFloat minFaviconSize = [UIScreen mainScreen].scale * self.minSize;
  self.largeIconService->GetLargeIconOrFallbackStyle(
      URL, minFaviconSize, faviconSize, base::BindBlockArc(faviconBlock),
      &_faviconTaskTracker);
}
@end
