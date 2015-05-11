// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/crw_session_entry.h"

#include "base/mac/objc_property_releaser.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/web/navigation/navigation_item_impl.h"
#include "ios/web/navigation/nscoder_util.h"
#include "ios/web/public/navigation_item.h"
#include "ios/web/public/web_state/page_scroll_state.h"
#import "net/base/mac/url_conversions.h"

namespace web {
// Keys used to serialize web::PageScrollState properties.
NSString* const kSessionEntryPageScrollStateKey = @"state";
NSString* const kSessionEntryScrollOffsetXKey = @"scrollX";
NSString* const kSessionEntryScrollOffsetYKey = @"scrollY";
NSString* const kSessionEntryMinimumZoomScaleKey = @"minZoom";
NSString* const kSessionEntryMaximumZoomScaleKey = @"maxZoom";
NSString* const kSessionEntryZoomScaleKey = @"zoom";

// Keys used to serialize navigation properties.
NSString* const kSessionEntryURLKey = @"virtualUrlString";
NSString* const kSessionEntryURLDeperecatedKey = @"virtualUrl";
NSString* const kSessionEntryReferrerURLKey = @"referrerUrlString";
NSString* const kSessionEntryReferrerURLDeprecatedKey = @"referrer";
NSString* const kSessionEntryReferrerPolicyKey = @"referrerPolicy";
NSString* const kSessionEntryTimestampKey = @"timestamp";
NSString* const kSessionEntryTitleKey = @"title";
NSString* const kSessionEntryPOSTDataKey = @"POSTData";
NSString* const kSessionEntryHTTPRequestHeadersKey = @"httpHeaders";
NSString* const kSessionEntrySkipResubmitConfirmationKey =
    @"skipResubmitDataConfirmation";
NSString* const kSessionEntryUseDesktopUserAgentKey = @"useDesktopUserAgent";
}

@interface CRWSessionEntry () {
  // The original URL of the page.  In cases where a redirect occurred, |url_|
  // will contain the final post-redirect URL, and |originalUrl_| will contain
  // the pre-redirect URL.  This field is not persisted to disk.
  GURL _originalUrl;

  // The NavigationItemImpl corresponding to this CRWSessionEntry.
  // TODO(stuartmorgan): Move ownership to NavigationManagerImpl.
  scoped_ptr<web::NavigationItemImpl> _navigationItem;

  base::mac::ObjCPropertyReleaser _propertyReleaser_CRWSessionEntry;
}
// Redefine originalUrl to be read-write.
@property(nonatomic, readwrite) const GURL& originalUrl;

// Converts a serialized NSDictionary to a web::PageScrollState.
+ (web::PageScrollState)scrollStateFromDictionary:(NSDictionary*)dictionary;
// Serializes a web::PageScrollState to an NSDictionary.
+ (NSDictionary*)dictionaryFromScrollState:
    (const web::PageScrollState&)scrollState;
// Returns a readable description of |scrollState|.
+ (NSString*)scrollStateDescription:(const web::PageScrollState&)scrollState;
@end

@implementation CRWSessionEntry

@synthesize originalUrl = _originalUrl;

- (instancetype)initWithNavigationItem:(scoped_ptr<web::NavigationItem>)item {
  self = [super init];
  if (self) {
    _propertyReleaser_CRWSessionEntry.Init(self, [CRWSessionEntry class]);
    _navigationItem.reset(
        static_cast<web::NavigationItemImpl*>(item.release()));
    self.originalUrl = _navigationItem->GetURL();
  }
  return self;
}

- (instancetype)initWithCoder:(NSCoder*)aDecoder {
  self = [super init];
  if (self) {
    _propertyReleaser_CRWSessionEntry.Init(self, [CRWSessionEntry class]);
    _navigationItem.reset(new web::NavigationItemImpl());

    // Desktop chrome only persists virtualUrl_ and uses it to feed the url
    // when creating a NavigationEntry.
    GURL url;
    if ([aDecoder containsValueForKey:web::kSessionEntryURLKey]) {
      url = GURL(
          web::nscoder_util::DecodeString(aDecoder, web::kSessionEntryURLKey));
    } else {
      // Backward compatibility.
      url = net::GURLWithNSURL(
          [aDecoder decodeObjectForKey:web::kSessionEntryURLDeperecatedKey]);
    }
    _navigationItem->SetURL(url);
    self.originalUrl = url;

    if ([aDecoder containsValueForKey:web::kSessionEntryReferrerURLKey]) {
      const std::string referrerString(web::nscoder_util::DecodeString(
          aDecoder, web::kSessionEntryReferrerURLKey));
      web::ReferrerPolicy referrerPolicy = static_cast<web::ReferrerPolicy>(
          [aDecoder decodeIntForKey:web::kSessionEntryReferrerPolicyKey]);
      _navigationItem->SetReferrer(
          web::Referrer(GURL(referrerString), referrerPolicy));
    } else {
      // Backward compatibility.
      NSURL* referrer = [aDecoder
          decodeObjectForKey:web::kSessionEntryReferrerURLDeprecatedKey];
      _navigationItem->SetReferrer(web::Referrer(
          net::GURLWithNSURL(referrer), web::ReferrerPolicyDefault));
    }

    if ([aDecoder containsValueForKey:web::kSessionEntryTimestampKey]) {
      int64 us = [aDecoder decodeInt64ForKey:web::kSessionEntryTimestampKey];
      _navigationItem->SetTimestamp(base::Time::FromInternalValue(us));
    }

    NSString* title = [aDecoder decodeObjectForKey:web::kSessionEntryTitleKey];
    // Use a transition type of reload so that we don't incorrectly increase
    // the typed count.  This is what desktop chrome does.
    _navigationItem->SetPageID(-1);
    _navigationItem->SetTitle(base::SysNSStringToUTF16(title));
    _navigationItem->SetTransitionType(ui::PAGE_TRANSITION_RELOAD);
    _navigationItem->SetPageScrollState([[self class]
        scrollStateFromDictionary:
            [aDecoder
                decodeObjectForKey:web::kSessionEntryPageScrollStateKey]]);
    _navigationItem->SetShouldSkipResubmitDataConfirmation([aDecoder
        decodeBoolForKey:web::kSessionEntrySkipResubmitConfirmationKey]);
    _navigationItem->SetIsOverridingUserAgent(
        [aDecoder decodeBoolForKey:web::kSessionEntryUseDesktopUserAgentKey]);
    _navigationItem->SetPostData(
        [aDecoder decodeObjectForKey:web::kSessionEntryPOSTDataKey]);
    _navigationItem->AddHttpRequestHeaders(
        [aDecoder decodeObjectForKey:web::kSessionEntryHTTPRequestHeadersKey]);
  }
  return self;
}

- (void)encodeWithCoder:(NSCoder*)aCoder {
  // Desktop Chrome doesn't persist |url_| or |originalUrl_|, only
  // |virtualUrl_|.
  web::nscoder_util::EncodeString(aCoder, web::kSessionEntryURLKey,
                                  _navigationItem->GetVirtualURL().spec());
  web::nscoder_util::EncodeString(aCoder, web::kSessionEntryReferrerURLKey,
                                  _navigationItem->GetReferrer().url.spec());
  [aCoder encodeInt:_navigationItem->GetReferrer().policy
             forKey:web::kSessionEntryReferrerPolicyKey];
  [aCoder encodeInt64:_navigationItem->GetTimestamp().ToInternalValue()
               forKey:web::kSessionEntryTimestampKey];

  [aCoder encodeObject:base::SysUTF16ToNSString(_navigationItem->GetTitle())
                forKey:web::kSessionEntryTitleKey];
  [aCoder encodeObject:[[self class] dictionaryFromScrollState:
                                         _navigationItem->GetPageScrollState()]
                forKey:web::kSessionEntryPageScrollStateKey];
  [aCoder encodeBool:_navigationItem->ShouldSkipResubmitDataConfirmation()
              forKey:web::kSessionEntrySkipResubmitConfirmationKey];
  [aCoder encodeBool:_navigationItem->IsOverridingUserAgent()
              forKey:web::kSessionEntryUseDesktopUserAgentKey];
  [aCoder encodeObject:_navigationItem->GetPostData()
                forKey:web::kSessionEntryPOSTDataKey];
  [aCoder encodeObject:_navigationItem->GetHttpRequestHeaders()
                forKey:web::kSessionEntryHTTPRequestHeadersKey];
}

// TODO(ios): Shall we overwrite EqualTo:?

- (instancetype)copyWithZone:(NSZone*)zone {
  CRWSessionEntry* copy = [[[self class] alloc] init];
  copy->_propertyReleaser_CRWSessionEntry.Init(copy, [CRWSessionEntry class]);
  copy->_navigationItem.reset(
      new web::NavigationItemImpl(*_navigationItem.get()));
  copy->_originalUrl = _originalUrl;
  return copy;
}

- (NSString*)description {
  return [NSString
      stringWithFormat:
          @"url:%@ originalurl:%@ title:%@ transition:%d scrollState:%@ "
          @"desktopUA:%d",
          base::SysUTF8ToNSString(_navigationItem->GetURL().spec()),
          base::SysUTF8ToNSString(self.originalUrl.spec()),
          base::SysUTF16ToNSString(_navigationItem->GetTitle()),
          _navigationItem->GetTransitionType(),
          [[self class]
              scrollStateDescription:_navigationItem->GetPageScrollState()],
          _navigationItem->IsOverridingUserAgent()];
}

- (web::NavigationItem*)navigationItem {
  return _navigationItem.get();
}

- (web::NavigationItemImpl*)navigationItemImpl {
  return _navigationItem.get();
}

#pragma mark - Serialization helpers

+ (web::PageScrollState)scrollStateFromDictionary:(NSDictionary*)dictionary {
  web::PageScrollState scrollState;
  NSNumber* serializedValue = nil;
  if ((serializedValue = dictionary[web::kSessionEntryScrollOffsetXKey]))
    scrollState.set_scroll_offset_x([serializedValue doubleValue]);
  if ((serializedValue = dictionary[web::kSessionEntryScrollOffsetYKey]))
    scrollState.set_scroll_offset_y([serializedValue doubleValue]);
  if ((serializedValue = dictionary[web::kSessionEntryMinimumZoomScaleKey]))
    scrollState.set_minimum_zoom_scale([serializedValue doubleValue]);
  if ((serializedValue = dictionary[web::kSessionEntryMaximumZoomScaleKey]))
    scrollState.set_maximum_zoom_scale([serializedValue doubleValue]);
  if ((serializedValue = dictionary[web::kSessionEntryZoomScaleKey]))
    scrollState.set_zoom_scale([serializedValue doubleValue]);
  return scrollState;
}

+ (NSDictionary*)dictionaryFromScrollState:
    (const web::PageScrollState&)scrollState {
  return @{
    web::kSessionEntryScrollOffsetXKey : @(scrollState.scroll_offset_x()),
    web::kSessionEntryScrollOffsetYKey : @(scrollState.scroll_offset_y()),
    web::kSessionEntryMinimumZoomScaleKey : @(scrollState.minimum_zoom_scale()),
    web::kSessionEntryMaximumZoomScaleKey : @(scrollState.maximum_zoom_scale()),
    web::kSessionEntryZoomScaleKey : @(scrollState.zoom_scale()),
  };
}

+ (NSString*)scrollStateDescription:(const web::PageScrollState&)scrollState {
  NSString* const kPageScrollStateDescriptionFormat =
      @"{ scrollOffset:(%0.2f, %0.2f), zoomScaleRange:(%0.2f, %0.2f), "
      @"zoomScale:%0.2f }";
  return [NSString stringWithFormat:kPageScrollStateDescriptionFormat,
                                    scrollState.scroll_offset_x(),
                                    scrollState.scroll_offset_y(),
                                    scrollState.minimum_zoom_scale(),
                                    scrollState.maximum_zoom_scale(),
                                    scrollState.zoom_scale()];
}

@end
