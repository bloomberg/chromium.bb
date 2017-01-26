// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/crw_navigation_manager_storage.h"

#import "ios/web/navigation/crw_session_certificate_policy_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Serialization keys used in NSCoding functions.
NSString* const kCertificatePolicyManagerKey = @"certificatePolicyManager";
NSString* const kCurrentNavigationIndexKey = @"currentNavigationIndex";
NSString* const kEntriesKey = @"entries";
NSString* const kLastVisitedTimestampKey = @"lastVisitedTimestamp";
NSString* const kOpenerIDKey = @"openerId";
NSString* const kOpenedByDOMKey = @"openedByDOM";
NSString* const kOpenerNavigationIndexKey = @"openerNavigationIndex";
NSString* const kPreviousNavigationIndexKey = @"previousNavigationIndex";
NSString* const kTabIDKey = @"tabId";
NSString* const kWindowNameKey = @"windowName";
}

@implementation CRWNavigationManagerStorage

@synthesize tabID = _tabID;
@synthesize openerID = _openerID;
@synthesize openedByDOM = _openedByDOM;
@synthesize openerNavigationIndex = _openerNavigationIndex;
@synthesize windowName = _windowName;
@synthesize currentNavigationIndex = _currentNavigationIndex;
@synthesize previousNavigationIndex = _previousNavigationIndex;
@synthesize lastVisitedTimestamp = _lastVisitedTimestamp;
@synthesize entries = _entries;
@synthesize sessionCertificatePolicyManager = _sessionCertificatePolicyManager;

- (instancetype)initWithCoder:(nonnull NSCoder*)decoder {
  self = [super init];
  if (self) {
    _tabID = [[decoder decodeObjectForKey:kTabIDKey] copy];
    _windowName = [[decoder decodeObjectForKey:kWindowNameKey] copy];
    _openerID = [[decoder decodeObjectForKey:kOpenerIDKey] copy];
    _openedByDOM = [decoder decodeBoolForKey:kOpenedByDOMKey];
    _openerNavigationIndex =
        [decoder decodeIntForKey:kOpenerNavigationIndexKey];
    _currentNavigationIndex =
        [decoder decodeIntForKey:kCurrentNavigationIndexKey];
    _previousNavigationIndex =
        [decoder decodeIntForKey:kPreviousNavigationIndexKey];
    _lastVisitedTimestamp =
        [decoder decodeDoubleForKey:kLastVisitedTimestampKey];
    _entries = [[NSMutableArray alloc]
        initWithArray:[decoder decodeObjectForKey:kEntriesKey]];
    // Prior to M34, 0 was used as "no index" instead of -1; adjust for that.
    if (!_entries.count)
      _currentNavigationIndex = -1;
    _sessionCertificatePolicyManager =
        [decoder decodeObjectForKey:kCertificatePolicyManagerKey];
    if (!_sessionCertificatePolicyManager) {
      _sessionCertificatePolicyManager =
          [[CRWSessionCertificatePolicyManager alloc] init];
    }
  }
  return self;
}

- (void)encodeWithCoder:(NSCoder*)coder {
  [coder encodeObject:self.tabID forKey:kTabIDKey];
  [coder encodeObject:self.openerID forKey:kOpenerIDKey];
  [coder encodeBool:self.openedByDOM forKey:kOpenedByDOMKey];
  [coder encodeInt:self.openerNavigationIndex forKey:kOpenerNavigationIndexKey];
  [coder encodeObject:self.windowName forKey:kWindowNameKey];
  [coder encodeInt:self.currentNavigationIndex
            forKey:kCurrentNavigationIndexKey];
  [coder encodeInt:self.previousNavigationIndex
            forKey:kPreviousNavigationIndexKey];
  [coder encodeDouble:self.lastVisitedTimestamp
               forKey:kLastVisitedTimestampKey];
  [coder encodeObject:self.entries forKey:kEntriesKey];
  [coder encodeObject:self.sessionCertificatePolicyManager
               forKey:kCertificatePolicyManagerKey];
  // rendererInitiated is deliberately not preserved, as upstream.
}

@end
