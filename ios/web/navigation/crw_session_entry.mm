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

namespace {
// Keys used to serialize web::PageScrollState properties.
NSString* const kScrollOffsetXKey = @"scrollX";
NSString* const kScrollOffsetYKey = @"scrollY";
NSString* const kMinimumZoomScaleKey = @"minZoom";
NSString* const kMaximumZoomScaleKey = @"maxZoom";
NSString* const kZoomScaleKey = @"zoom";
}

@interface CRWSessionEntry () {
  // The index in the CRWSessionController.
  //
  // This is used when determining the selected CRWSessionEntry and only useful
  // to the SessionServiceIOS.
  NSInteger _index;

  // The original URL of the page.  In cases where a redirect occurred, |url_|
  // will contain the final post-redirect URL, and |originalUrl_| will contain
  // the pre-redirect URL.  This field is not persisted to disk.
  GURL _originalUrl;

  // Headers passed along with the request. For POST requests, these are
  // persisted, to be able to resubmit them. Some specialized non-POST requests
  // may also pass custom headers.
  base::scoped_nsobject<NSMutableDictionary> _httpHeaders;

  // Data submitted with a POST request, persisted for resubmits.
  NSData* _POSTData;

  // Serialized representation of the state object that was used in conjunction
  // with a JavaScript window.history.pushState() or
  // window.history.replaceState() call that created or modified this
  // CRWSessionEntry. Intended to be used for JavaScript history operations and
  // will be nil in most cases.
  NSString* _serializedStateObject;

  // Whether or not this entry was created by calling history.pushState().
  BOOL _createdFromPushState;

  // If |YES| use a desktop user agent in HTTP requests and UIWebView.
  BOOL _useDesktopUserAgent;

  // If |YES| the page was last fetched through the data reduction proxy.
  BOOL _usedDataReductionProxy;

  // Whether or not to bypass showing the resubmit data confirmation when
  // loading a POST request. Set to YES for browser-generated POST requests such
  // as search-by-image requests.
  BOOL _skipResubmitDataConfirmation;

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

@synthesize POSTData = _POSTData;
@synthesize originalUrl = _originalUrl;
@synthesize useDesktopUserAgent = _useDesktopUserAgent;
@synthesize usedDataReductionProxy = _usedDataReductionProxy;
@synthesize index = _index;
@synthesize serializedStateObject = _serializedStateObject;
@synthesize createdFromPushState = _createdFromPushState;
@synthesize skipResubmitDataConfirmation = _skipResubmitDataConfirmation;

// Creates a new session entry. These may be nil.
- (instancetype)initWithUrl:(const GURL&)url
                   referrer:(const web::Referrer&)referrer
                 transition:(ui::PageTransition)transition
        useDesktopUserAgent:(BOOL)useDesktopUserAgent
          rendererInitiated:(BOOL)rendererInitiated {
  self = [super init];
  if (self) {
    _propertyReleaser_CRWSessionEntry.Init(self, [CRWSessionEntry class]);
    _navigationItem.reset(new web::NavigationItemImpl());

    _navigationItem->SetURL(url);
    _navigationItem->SetReferrer(referrer);
    _navigationItem->SetTransitionType(transition);
    _navigationItem->set_is_renderer_initiated(rendererInitiated);

    self.originalUrl = url;
    self.useDesktopUserAgent = useDesktopUserAgent;
  }
  return self;
}

- (instancetype)initWithNavigationItem:(scoped_ptr<web::NavigationItem>)item
                                 index:(int)index {
  self = [super init];
  if (self) {
    _propertyReleaser_CRWSessionEntry.Init(self, [CRWSessionEntry class]);
    _navigationItem.reset(
        static_cast<web::NavigationItemImpl*>(item.release()));

    self.index = index;
    self.originalUrl = _navigationItem->GetURL();
    self.useDesktopUserAgent = NO;
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
    if ([aDecoder containsValueForKey:@"virtualUrlString"]) {
      url = GURL(
          web::nscoder_util::DecodeString(aDecoder, @"virtualUrlString"));
    } else {
      // Backward compatibility.
      url = net::GURLWithNSURL([aDecoder decodeObjectForKey:@"virtualUrl"]);
    }
    _navigationItem->SetURL(url);
    self.originalUrl = url;

    if ([aDecoder containsValueForKey:@"referrerUrlString"]) {
      const std::string referrerString(web::nscoder_util::DecodeString(
          aDecoder, @"referrerUrlString"));
      web::ReferrerPolicy referrerPolicy =
          static_cast<web::ReferrerPolicy>(
              [aDecoder decodeIntForKey:@"referrerPolicy"]);
      _navigationItem->SetReferrer(
          web::Referrer(GURL(referrerString), referrerPolicy));
    } else {
      // Backward compatibility.
      NSURL* referrer = [aDecoder decodeObjectForKey:@"referrer"];
      _navigationItem->SetReferrer(web::Referrer(
          net::GURLWithNSURL(referrer), web::ReferrerPolicyDefault));
    }

    if ([aDecoder containsValueForKey:@"timestamp"]) {
      int64 us = [aDecoder decodeInt64ForKey:@"timestamp"];
      _navigationItem->SetTimestamp(base::Time::FromInternalValue(us));
    }

    NSString* title = [aDecoder decodeObjectForKey:@"title"];
    // Use a transition type of reload so that we don't incorrectly increase
    // the typed count.  This is what desktop chrome does.
    _navigationItem->SetPageID(-1);
    _navigationItem->SetTitle(base::SysNSStringToUTF16(title));
    _navigationItem->SetTransitionType(ui::PAGE_TRANSITION_RELOAD);
    _navigationItem->SetPageScrollState([[self class]
        scrollStateFromDictionary:[aDecoder decodeObjectForKey:@"state"]]);
    self.index = [aDecoder decodeIntForKey:@"index"];
    self.useDesktopUserAgent =
        [aDecoder decodeBoolForKey:@"useDesktopUserAgent"];
    self.usedDataReductionProxy =
        [aDecoder decodeBoolForKey:@"usedDataReductionProxy"];
    [self addHTTPHeaders:[aDecoder decodeObjectForKey:@"httpHeaders"]];
    self.POSTData = [aDecoder decodeObjectForKey:@"POSTData"];
    self.skipResubmitDataConfirmation =
        [aDecoder decodeBoolForKey:@"skipResubmitDataConfirmation"];
  }
  return self;
}

- (void)encodeWithCoder:(NSCoder*)aCoder {
  // Desktop Chrome doesn't persist |url_| or |originalUrl_|, only
  // |virtualUrl_|.
  [aCoder encodeInt:self.index forKey:@"index"];
  web::nscoder_util::EncodeString(aCoder, @"virtualUrlString",
                                  _navigationItem->GetVirtualURL().spec());
  web::nscoder_util::EncodeString(aCoder, @"referrerUrlString",
                                  _navigationItem->GetReferrer().url.spec());
  [aCoder encodeInt:_navigationItem->GetReferrer().policy
             forKey:@"referrerPolicy"];
  [aCoder encodeInt64:_navigationItem->GetTimestamp().ToInternalValue()
               forKey:@"timestamp"];

  [aCoder encodeObject:base::SysUTF16ToNSString(_navigationItem->GetTitle())
                forKey:@"title"];
  [aCoder encodeObject:[[self class] dictionaryFromScrollState:
                                         _navigationItem->GetPageScrollState()]
                forKey:@"state"];
  [aCoder encodeBool:self.useDesktopUserAgent forKey:@"useDesktopUserAgent"];
  [aCoder encodeBool:self.usedDataReductionProxy
              forKey:@"usedDataReductionProxy"];
  [aCoder encodeObject:self.httpHeaders forKey:@"httpHeaders"];
  [aCoder encodeObject:self.POSTData forKey:@"POSTData"];
  [aCoder encodeBool:self.skipResubmitDataConfirmation
              forKey:@"skipResubmitDataConfirmation"];
}

// TODO(ios): Shall we overwrite EqualTo:?

- (instancetype)copyWithZone:(NSZone*)zone {
  CRWSessionEntry* copy = [[[self class] alloc] init];
  copy->_propertyReleaser_CRWSessionEntry.Init(copy, [CRWSessionEntry class]);
  copy->_navigationItem.reset(
      new web::NavigationItemImpl(*_navigationItem.get()));
  copy->_index = _index;
  copy->_originalUrl = _originalUrl;
  copy->_useDesktopUserAgent = _useDesktopUserAgent;
  copy->_usedDataReductionProxy = _usedDataReductionProxy;
  copy->_POSTData = [_POSTData copy];
  copy->_httpHeaders.reset([_httpHeaders mutableCopy]);
  copy->_skipResubmitDataConfirmation = _skipResubmitDataConfirmation;
  return copy;
}

- (NSString*)description {
  return [NSString
      stringWithFormat:
          @"url:%@ originalurl:%@ title:%@ transition:%d scrollState:%@ "
          @"desktopUA:%d " @"proxy:%d",
          base::SysUTF8ToNSString(_navigationItem->GetURL().spec()),
          base::SysUTF8ToNSString(self.originalUrl.spec()),
          base::SysUTF16ToNSString(_navigationItem->GetTitle()),
          _navigationItem->GetTransitionType(),
          [[self class]
              scrollStateDescription:_navigationItem->GetPageScrollState()],
          _useDesktopUserAgent, _usedDataReductionProxy];
}

- (web::NavigationItem*)navigationItem {
  return _navigationItem.get();
}

- (NSDictionary*)httpHeaders {
  return _httpHeaders ? [NSDictionary dictionaryWithDictionary:_httpHeaders]
                      : nil;
}

- (void)addHTTPHeaders:(NSDictionary*)moreHTTPHeaders {
  if (_httpHeaders)
    [_httpHeaders addEntriesFromDictionary:moreHTTPHeaders];
  else
    _httpHeaders.reset([moreHTTPHeaders mutableCopy]);
}

- (void)removeHTTPHeaderForKey:(NSString*)key {
  [_httpHeaders removeObjectForKey:key];
  if (![_httpHeaders count])
    _httpHeaders.reset();
}

- (void)resetHTTPHeaders {
  _httpHeaders.reset();
}

#pragma mark - Serialization helpers

+ (web::PageScrollState)scrollStateFromDictionary:(NSDictionary*)dictionary {
  web::PageScrollState scrollState;
  NSNumber* serializedValue = nil;
  if ((serializedValue = dictionary[kScrollOffsetXKey]))
    scrollState.set_scroll_offset_x([serializedValue doubleValue]);
  if ((serializedValue = dictionary[kScrollOffsetYKey]))
    scrollState.set_scroll_offset_y([serializedValue doubleValue]);
  if ((serializedValue = dictionary[kMinimumZoomScaleKey]))
    scrollState.set_minimum_zoom_scale([serializedValue doubleValue]);
  if ((serializedValue = dictionary[kMaximumZoomScaleKey]))
    scrollState.set_maximum_zoom_scale([serializedValue doubleValue]);
  if ((serializedValue = dictionary[kZoomScaleKey]))
    scrollState.set_zoom_scale([serializedValue doubleValue]);
  return scrollState;
}

+ (NSDictionary*)dictionaryFromScrollState:
    (const web::PageScrollState&)scrollState {
  return @{
    kScrollOffsetXKey : @(scrollState.scroll_offset_x()),
    kScrollOffsetYKey : @(scrollState.scroll_offset_y()),
    kMinimumZoomScaleKey : @(scrollState.minimum_zoom_scale()),
    kMaximumZoomScaleKey : @(scrollState.maximum_zoom_scale()),
    kZoomScaleKey : @(scrollState.zoom_scale())
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
