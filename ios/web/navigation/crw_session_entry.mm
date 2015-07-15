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
#include "ios/web/public/web_state/page_display_state.h"
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

// Converts a serialized NSDictionary to a web::PageDisplayState.
+ (web::PageDisplayState)pageDisplayStateFromDictionary:
        (NSDictionary*)dictionary;
// Serializes a web::PageDisplayState to an NSDictionary.
+ (NSDictionary*)dictionaryFromPageDisplayState:
        (const web::PageDisplayState&)displayState;
// Returns a readable description of |displayState|.
+ (NSString*)descriptionForPageDisplayState:
        (const web::PageDisplayState&)displayState;
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
    _navigationItem->SetTitle(base::SysNSStringToUTF16(title));
    _navigationItem->SetTransitionType(ui::PAGE_TRANSITION_RELOAD);
    _navigationItem->SetPageDisplayState([[self class]
        pageDisplayStateFromDictionary:
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
  [aCoder encodeObject:[[self class] dictionaryFromPageDisplayState:
                                         _navigationItem->GetPageDisplayState()]
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
          @"url:%@ originalurl:%@ title:%@ transition:%d displayState:%@ "
          @"desktopUA:%d",
          base::SysUTF8ToNSString(_navigationItem->GetURL().spec()),
          base::SysUTF8ToNSString(self.originalUrl.spec()),
          base::SysUTF16ToNSString(_navigationItem->GetTitle()),
          _navigationItem->GetTransitionType(),
          [[self class] descriptionForPageDisplayState:
                            _navigationItem->GetPageDisplayState()],
          _navigationItem->IsOverridingUserAgent()];
}

- (web::NavigationItem*)navigationItem {
  return _navigationItem.get();
}

- (web::NavigationItemImpl*)navigationItemImpl {
  return _navigationItem.get();
}

#pragma mark - Serialization helpers

+ (web::PageDisplayState)pageDisplayStateFromDictionary:
    (NSDictionary*)dictionary {
  NSNumber* serializedValue = nil;
  web::PageScrollState scrollState;
  if ((serializedValue = dictionary[web::kSessionEntryScrollOffsetXKey]))
    scrollState.set_offset_x([serializedValue doubleValue]);
  if ((serializedValue = dictionary[web::kSessionEntryScrollOffsetYKey]))
    scrollState.set_offset_y([serializedValue doubleValue]);
  web::PageZoomState zoomState;
  if ((serializedValue = dictionary[web::kSessionEntryMinimumZoomScaleKey]))
    zoomState.set_minimum_zoom_scale([serializedValue doubleValue]);
  if ((serializedValue = dictionary[web::kSessionEntryMaximumZoomScaleKey]))
    zoomState.set_maximum_zoom_scale([serializedValue doubleValue]);
  if ((serializedValue = dictionary[web::kSessionEntryZoomScaleKey]))
    zoomState.set_zoom_scale([serializedValue doubleValue]);
  return web::PageDisplayState(scrollState, zoomState);
}

+ (NSDictionary*)dictionaryFromPageDisplayState:
    (const web::PageDisplayState&)displayState {
  return @{
    web::kSessionEntryScrollOffsetXKey :
        @(displayState.scroll_state().offset_x()),
    web::kSessionEntryScrollOffsetYKey :
        @(displayState.scroll_state().offset_y()),
    web::kSessionEntryMinimumZoomScaleKey :
        @(displayState.zoom_state().minimum_zoom_scale()),
    web::kSessionEntryMaximumZoomScaleKey :
        @(displayState.zoom_state().maximum_zoom_scale()),
    web::kSessionEntryZoomScaleKey :
        @(displayState.zoom_state().zoom_scale())
  };
}

+ (NSString*)descriptionForPageDisplayState:
        (const web::PageDisplayState&)displayState {
  NSString* const kPageScrollStateDescriptionFormat =
      @"{ scrollOffset:(%0.2f, %0.2f), zoomScaleRange:(%0.2f, %0.2f), "
      @"zoomScale:%0.2f }";
  return
      [NSString stringWithFormat:kPageScrollStateDescriptionFormat,
                                 displayState.scroll_state().offset_x(),
                                 displayState.scroll_state().offset_y(),
                                 displayState.zoom_state().minimum_zoom_scale(),
                                 displayState.zoom_state().maximum_zoom_scale(),
                                 displayState.zoom_state().zoom_scale()];
}

@end
