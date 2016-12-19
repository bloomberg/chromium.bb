// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/history/favicon_view_provider.h"

#include "base/i18n/case_conversion.h"
#include "base/ios/weak_nsobject.h"
#include "base/mac/bind_objc_block.h"
#import "base/mac/foundation_util.h"
#include "base/mac/objc_property_releaser.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/favicon/core/fallback_url_util.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/favicon_base/fallback_icon_style.h"
#include "components/favicon_base/favicon_types.h"
#import "ios/chrome/browser/ui/history/favicon_view.h"
#import "ios/third_party/material_roboto_font_loader_ios/src/src/MaterialRobotoFontLoader.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "skia/ext/skia_utils_ios.h"
#include "url/gurl.h"

@interface FaviconViewProvider () {
  // Property releaser for FaviconViewProvider.
  base::mac::ObjCPropertyReleaser _propertyReleaser_FaviconViewProvider;
  // Delegate for handling completion of favicon load.
  base::WeakNSProtocol<id<FaviconViewProviderDelegate>> _delegate;
  // Used to cancel tasks for the LargeIconService.
  base::CancelableTaskTracker _faviconTaskTracker;
  // View that renders a favicon or a fallback image.
  base::scoped_nsobject<FaviconView> _faviconView;
}

// Size to render the favicon.
@property(nonatomic, assign) CGFloat faviconSize;
// Favicon image for the favicon view.
@property(nonatomic, retain) UIImage* favicon;
// Fallback text for the favicon view if there is no appropriately sized
// favicon availabile.
@property(nonatomic, copy) NSString* fallbackText;
// Fallback background color for the favicon view if there is no appropriately
// sized favicon available.
@property(nonatomic, retain) UIColor* fallbackBackgroundColor;
// Fallback text color for the favicon view if there is no appropriately
// sized favicon available.
@property(nonatomic, retain) UIColor* fallbackTextColor;

// Fetches favicon for |URL| from |faviconService|. Notifies delegate when
// favicon is retrieved.
- (void)fetchFaviconForURL:(const GURL&)URL
                      size:(CGFloat)size
                   minSize:(CGFloat)minSize
                   service:(favicon::LargeIconService*)faviconService;

@end

@implementation FaviconViewProvider

@synthesize faviconSize = _faviconSize;
@synthesize favicon = _favicon;
@synthesize fallbackText = _fallbackText;
@synthesize fallbackBackgroundColor = _fallbackBackgroundColor;
@synthesize fallbackTextColor = _fallbackTextColor;

- (instancetype)initWithURL:(const GURL&)URL
                faviconSize:(CGFloat)faviconSize
             minFaviconSize:(CGFloat)minFaviconSize
           largeIconService:(favicon::LargeIconService*)largeIconService
                   delegate:(id<FaviconViewProviderDelegate>)delegate {
  self = [super init];
  if (self) {
    _propertyReleaser_FaviconViewProvider.Init(self,
                                               [FaviconViewProvider class]);
    _faviconSize = faviconSize;
    _delegate.reset(delegate);
    _fallbackBackgroundColor = [[UIColor grayColor] retain];
    _fallbackTextColor = [[UIColor whiteColor] retain];
    [self fetchFaviconForURL:URL
                        size:faviconSize
                     minSize:minFaviconSize
                     service:largeIconService];
  }
  return self;
}

- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (void)fetchFaviconForURL:(const GURL&)URL
                      size:(CGFloat)size
                   minSize:(CGFloat)minSize
                   service:(favicon::LargeIconService*)largeIconService {
  if (!largeIconService)
    return;
  base::WeakNSObject<FaviconViewProvider> weakSelf(self);
  GURL blockURL(URL);
  void (^faviconBlock)(const favicon_base::LargeIconResult&) = ^(
      const favicon_base::LargeIconResult& result) {
    base::scoped_nsobject<FaviconViewProvider> strongSelf([weakSelf retain]);
    if (!strongSelf)
      return;
    if (result.bitmap.is_valid()) {
      scoped_refptr<base::RefCountedMemory> data =
          result.bitmap.bitmap_data.get();
      [strongSelf
          setFavicon:[UIImage
                         imageWithData:[NSData dataWithBytes:data->front()
                                                      length:data->size()]]];
    } else if (result.fallback_icon_style) {
      [strongSelf setFallbackBackgroundColor:skia::UIColorFromSkColor(
                                                 result.fallback_icon_style
                                                     ->background_color)];
      [strongSelf
          setFallbackTextColor:skia::UIColorFromSkColor(
                                   result.fallback_icon_style->text_color)];
      [strongSelf setFallbackText:base::SysUTF16ToNSString(
                                      favicon::GetFallbackIconText(blockURL))];
    }
    [strongSelf.get()->_delegate faviconViewProviderFaviconDidLoad:strongSelf];
  };

  // Always call LargeIconService in case the favicon was updated.
  CGFloat faviconSize = [UIScreen mainScreen].scale * size;
  CGFloat minFaviconSize = [UIScreen mainScreen].scale * minSize;
  largeIconService->GetLargeIconOrFallbackStyle(
      URL, minFaviconSize, faviconSize, base::BindBlock(faviconBlock),
      &_faviconTaskTracker);
}

- (FaviconView*)faviconView {
  if (!_faviconView) {
    _faviconView.reset([[FaviconView alloc] initWithFrame:CGRectZero]);
  }
  _faviconView.get().size = _faviconSize;
  // Update favicon view with current properties.
  if (self.favicon) {
    _faviconView.get().faviconImage.image = self.favicon;
    _faviconView.get().faviconImage.backgroundColor = [UIColor whiteColor];
    _faviconView.get().faviconFallbackLabel.text = nil;
  } else {
    _faviconView.get().faviconImage.image = nil;
    _faviconView.get().faviconImage.backgroundColor =
        self.fallbackBackgroundColor;
    _faviconView.get().faviconFallbackLabel.text = self.fallbackText;
    _faviconView.get().faviconFallbackLabel.textColor = self.fallbackTextColor;

    CGFloat fontSize = floorf(_faviconSize / 2);
    _faviconView.get().faviconFallbackLabel.font =
        [[MDFRobotoFontLoader sharedInstance] regularFontOfSize:fontSize];
  }
  return _faviconView;
}

@end
