// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_CRW_SESSION_ENTRY_H_
#define IOS_WEB_NAVIGATION_CRW_SESSION_ENTRY_H_

#import <Foundation/Foundation.h>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace web {
class NavigationItem;
class NavigationItemImpl;
struct Referrer;

// Keys used to serialize web::PageScrollState properties.
extern NSString* const kSessionEntryPageScrollStateKey;
extern NSString* const kSessionEntryScrollOffsetXKey;
extern NSString* const kSessionEntryScrollOffsetYKey;
extern NSString* const kSessionEntryMinimumZoomScaleKey;
extern NSString* const kSessionEntryMaximumZoomScaleKey;
extern NSString* const kSessionEntryZoomScaleKey;

// Keys used to serialize navigation properties.

// Current URL (std::string).
extern NSString* const kSessionEntryURLKey;
// Current URL. Deprecated, used for backward compatibility (NSURL).
extern NSString* const kSessionEntryURLDeperecatedKey;
// Page referrer URL (std::string).
extern NSString* const kSessionEntryReferrerURLKey;
// Page referrer URL (NSURL). Deprecated, used for backward compatibility.
extern NSString* const kSessionEntryReferrerURLDeprecatedKey;
// Page referrer policy (int).
extern NSString* const kSessionEntryReferrerPolicyKey;
// The time at which the last known local navigation has completed (int64_t).
extern NSString* const kSessionEntryTimestampKey;
// Page title (NSString).
extern NSString* const kSessionEntryTitleKey;
// POST request data (NSData).
extern NSString* const kSessionEntryPOSTDataKey;
// HTTP request headers (NSDictionary).
extern NSString* const kSessionEntryHTTPRequestHeadersKey;
// Whether or not to bypass showing the resubmit data confirmation when loading
// a POST request (BOOL).
extern NSString* const kSessionEntrySkipResubmitConfirmationKey;
// Should desktop user agent be used (BOOL)?
extern NSString* const kSessionEntryUseDesktopUserAgentKey;
}

// A CRWSessionEntry is similar to a NavigationEntry object in desktop Chrome.
// It maintains the information needed to save/restore a single entry in session
// history (i.e., one site) for a tab. A tab may have multiple of these objects
// comprising its entire session history.
// TODO(rohitrao): Fold CRWSessionEntry's logic into NavigationItem.
@interface CRWSessionEntry : NSObject<NSCoding, NSCopying>

@property(nonatomic, readonly) const GURL& originalUrl;

// Pointer to the NavigationItem associated with this CRWSessionEntry.
// Eventually, this will replace CRWSessionEntry entirely.
@property(nonatomic, readonly) web::NavigationItem* navigationItem;

// Pointer to the NavigationItemImpl associated with this CRWSessionEntry.
@property(nonatomic, readonly) web::NavigationItemImpl* navigationItemImpl;

// Initialize the session entry with the given NavigationItem.
- (instancetype)initWithNavigationItem:(scoped_ptr<web::NavigationItem>)item;

@end

#endif  // IOS_WEB_NAVIGATION_CRW_SESSION_ENTRY_H_
