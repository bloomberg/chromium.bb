// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/spotlight/base_spotlight_manager.h"

#import <CommonCrypto/CommonCrypto.h>
#import <MobileCoreServices/MobileCoreServices.h>

#include "base/bind.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/favicon/core/fallback_url_util.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/favicon_base/fallback_icon_style.h"
#include "components/favicon_base/favicon_types.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/spotlight/spotlight_provider.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#import "net/base/mac/url_conversions.h"
#include "skia/ext/skia_utils_ios.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Minimum size of the icon to be used in Spotlight.
const NSInteger kMinIconSize = 32;

// Preferred size of the icon to be used in Spotlight.
const NSInteger kIconSize = 64;

// Size of the fallback icon.
const CGFloat kFallbackIconSize = 180;

// Radius of the rounded corner of the fallback icon.
const CGFloat kFallbackRoundedCorner = 8;
}

@interface BaseSpotlightManager () {
  // Domain of the spotlight manager.
  spotlight::Domain _spotlightDomain;

  // Service to retrieve large favicon or colors for a fallback icon.
  favicon::LargeIconService* _largeIconService;  // weak

  // Queue to query large icons.
  base::CancelableTaskTracker _largeIconTaskTracker;

  // Dictionary to track the tasks querying the large icons.
  NSMutableDictionary* _pendingTasks;

  // Records whether -shutdown has been invoked and the method forwarded to
  // the base class.
  BOOL _shutdownCalled;
}

// Compute a hash consisting of the first 8 bytes of the MD5 hash of a string
// containing |URL| and |title|.
- (int64_t)getHashForURL:(const GURL&)URL title:(NSString*)title;

// Create an image with a rounded square with color |backgroundColor| and
// |string| centered in color |textColor|.
UIImage* GetFallbackImageWithStringAndColor(NSString* string,
                                            UIColor* backgroundColor,
                                            UIColor* textColor);

// Returns an array of Keywords for Spotlight search.
- (NSArray*)keywordsForSpotlightItems;

@end

@implementation BaseSpotlightManager

- (instancetype)initWithLargeIconService:
                    (favicon::LargeIconService*)largeIconService
                                  domain:(spotlight::Domain)domain {
  self = [super init];
  if (self) {
    _spotlightDomain = domain;
    _largeIconService = largeIconService;
    _pendingTasks = [[NSMutableDictionary alloc] init];
  }
  return self;
}

- (void)dealloc {
  DCHECK(_shutdownCalled);
}

- (int64_t)getHashForURL:(const GURL&)URL title:(NSString*)title {
  NSString* key = [NSString
      stringWithFormat:@"%@ %@", base::SysUTF8ToNSString(URL.spec()), title];
  unsigned char hash[CC_MD5_DIGEST_LENGTH];
  const std::string clipboard = base::SysNSStringToUTF8(key);
  const char* c_string = clipboard.c_str();
  CC_MD5(c_string, strlen(c_string), hash);
  uint64_t md5 = *(reinterpret_cast<uint64_t*>(hash));
  return md5;
}

- (NSString*)spotlightIDForURL:(const GURL&)URL title:(NSString*)title {
  NSString* spotlightID = [NSString
      stringWithFormat:@"%@.%016llx",
                       spotlight::StringFromSpotlightDomain(_spotlightDomain),
                       [self getHashForURL:URL title:title]];
  return spotlightID;
}

- (void)cancelAllLargeIconPendingTasks {
  _largeIconTaskTracker.TryCancelAll();
  [_pendingTasks removeAllObjects];
}

- (void)clearAllSpotlightItems:(BlockWithError)callback {
  [self cancelAllLargeIconPendingTasks];
  spotlight::DeleteSearchableDomainItems(_spotlightDomain, callback);
}

- (CSSearchableItem*)spotlightItemWithItemID:(NSString*)itemID
                                attributeSet:(CSSearchableItemAttributeSet*)
                                                 attributeSet {
  CSCustomAttributeKey* key = [[CSCustomAttributeKey alloc]
          initWithKeyName:spotlight::GetSpotlightCustomAttributeItemID()
               searchable:YES
      searchableByDefault:YES
                   unique:YES
              multiValued:NO];
  [attributeSet setValue:itemID forCustomKey:key];
  attributeSet.keywords = [self keywordsForSpotlightItems];

  NSString* domainID = spotlight::StringFromSpotlightDomain(_spotlightDomain);

  return [[CSSearchableItem alloc] initWithUniqueIdentifier:itemID
                                           domainIdentifier:domainID
                                               attributeSet:attributeSet];
}

- (NSArray*)spotlightItemsWithURL:(const GURL&)indexedURL
                          favicon:(UIImage*)favicon
                     defaultTitle:(NSString*)defaultTitle {
  DCHECK(defaultTitle);
  NSURL* nsURL = net::NSURLWithGURL(indexedURL);
  std::string description = indexedURL.SchemeIsCryptographic()
                                ? indexedURL.GetOrigin().spec()
                                : indexedURL.spec();

  CSSearchableItemAttributeSet* attributeSet =
      [[CSSearchableItemAttributeSet alloc]
          initWithItemContentType:(NSString*)kUTTypeURL];
  [attributeSet setTitle:defaultTitle];
  [attributeSet setDisplayName:defaultTitle];
  [attributeSet setURL:nsURL];
  [attributeSet setContentURL:nsURL];
  [attributeSet setContentDescription:base::SysUTF8ToNSString(description)];
  [attributeSet setThumbnailData:UIImagePNGRepresentation(favicon)];

  NSString* itemID = [self spotlightIDForURL:indexedURL title:defaultTitle];
  return [NSArray arrayWithObject:[self spotlightItemWithItemID:itemID
                                                   attributeSet:attributeSet]];
}

UIImage* GetFallbackImageWithStringAndColor(NSString* string,
                                            UIColor* backgroundColor,
                                            UIColor* textColor) {
  CGRect rect = CGRectMake(0, 0, kFallbackIconSize, kFallbackIconSize);
  UIGraphicsBeginImageContext(rect.size);
  CGContextRef context = UIGraphicsGetCurrentContext();
  CGContextSetFillColorWithColor(context, [backgroundColor CGColor]);
  CGContextSetStrokeColorWithColor(context, [textColor CGColor]);
  UIBezierPath* rounded =
      [UIBezierPath bezierPathWithRoundedRect:rect
                                 cornerRadius:kFallbackRoundedCorner];
  [rounded fill];
  UIFont* font = [MDCTypography headlineFont];
  font = [font fontWithSize:(kFallbackIconSize / 2)];
  CGRect textRect = CGRectMake(0, (kFallbackIconSize - [font lineHeight]) / 2,
                               kFallbackIconSize, [font lineHeight]);
  NSMutableParagraphStyle* paragraphStyle =
      [[NSMutableParagraphStyle alloc] init];
  [paragraphStyle setAlignment:NSTextAlignmentCenter];
  NSMutableDictionary* attributes = [[NSMutableDictionary alloc] init];
  [attributes setValue:font forKey:NSFontAttributeName];
  [attributes setValue:textColor forKey:NSForegroundColorAttributeName];
  [attributes setValue:paragraphStyle forKey:NSParagraphStyleAttributeName];

  [string drawInRect:textRect withAttributes:attributes];
  UIImage* image = UIGraphicsGetImageFromCurrentImageContext();
  UIGraphicsEndImageContext();
  return image;
}

- (void)refreshItemsWithURL:(const GURL&)URLToRefresh title:(NSString*)title {
  NSURL* NSURL = net::NSURLWithGURL(URLToRefresh);

  if (!NSURL || [_pendingTasks objectForKey:NSURL]) {
    return;
  }

  __weak BaseSpotlightManager* weakSelf = self;
  GURL URL = URLToRefresh;
  void (^faviconBlock)(const favicon_base::LargeIconResult&) = ^(
      const favicon_base::LargeIconResult& result) {
    BaseSpotlightManager* strongSelf = weakSelf;
    if (!strongSelf) {
      return;
    }
    [strongSelf->_pendingTasks removeObjectForKey:NSURL];
    UIImage* favicon;
    if (result.bitmap.is_valid()) {
      scoped_refptr<base::RefCountedMemory> data =
          result.bitmap.bitmap_data.get();
      favicon = [UIImage
          imageWithData:[NSData dataWithBytes:data->front() length:data->size()]
                  scale:[UIScreen mainScreen].scale];
    } else {
      NSString* iconText =
          base::SysUTF16ToNSString(favicon::GetFallbackIconText(URL));
      UIColor* backgroundColor = skia::UIColorFromSkColor(
          result.fallback_icon_style->background_color);
      UIColor* textColor =
          skia::UIColorFromSkColor(result.fallback_icon_style->text_color);
      favicon = GetFallbackImageWithStringAndColor(iconText, backgroundColor,
                                                   textColor);
    }
    NSArray* spotlightItems = [strongSelf spotlightItemsWithURL:URL
                                                        favicon:favicon
                                                   defaultTitle:title];

    if ([spotlightItems count]) {
      [[CSSearchableIndex defaultSearchableIndex]
          indexSearchableItems:spotlightItems
             completionHandler:nil];
    }
  };

  base::CancelableTaskTracker::TaskId taskID =
      _largeIconService->GetLargeIconRawBitmapOrFallbackStyleForPageUrl(
          URL, kMinIconSize * [UIScreen mainScreen].scale,
          kIconSize * [UIScreen mainScreen].scale,
          base::BindRepeating(faviconBlock), &_largeIconTaskTracker);
  [_pendingTasks setObject:[NSNumber numberWithLongLong:taskID] forKey:NSURL];
}

- (NSUInteger)pendingLargeIconTasksCount {
  return [_pendingTasks count];
}

- (void)shutdown {
  [self cancelAllLargeIconPendingTasks];
  _largeIconService = nullptr;
  _shutdownCalled = YES;
}

#pragma mark private methods

- (NSArray*)keywordsForSpotlightItems {
  NSMutableArray* keywordsArray = [NSMutableArray arrayWithArray:@[
    l10n_util::GetNSString(IDS_IOS_SPOTLIGHT_KEYWORD_ONE),
    l10n_util::GetNSString(IDS_IOS_SPOTLIGHT_KEYWORD_TWO),
    l10n_util::GetNSString(IDS_IOS_SPOTLIGHT_KEYWORD_THREE),
    l10n_util::GetNSString(IDS_IOS_SPOTLIGHT_KEYWORD_FOUR),
    l10n_util::GetNSString(IDS_IOS_SPOTLIGHT_KEYWORD_FIVE),
    l10n_util::GetNSString(IDS_IOS_SPOTLIGHT_KEYWORD_SIX),
    l10n_util::GetNSString(IDS_IOS_SPOTLIGHT_KEYWORD_SEVEN),
    l10n_util::GetNSString(IDS_IOS_SPOTLIGHT_KEYWORD_EIGHT),
    l10n_util::GetNSString(IDS_IOS_SPOTLIGHT_KEYWORD_NINE),
    l10n_util::GetNSString(IDS_IOS_SPOTLIGHT_KEYWORD_TEN)
  ]];
  NSArray* additionalArray = ios::GetChromeBrowserProvider()
                                 ->GetSpotlightProvider()
                                 ->GetAdditionalKeywords();
  if (additionalArray) {
    [keywordsArray addObjectsFromArray:additionalArray];
  }
  return keywordsArray;
}

@end
