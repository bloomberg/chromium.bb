// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/crw_session_storage.h"

#import "ios/web/navigation/crw_session_certificate_policy_manager.h"
#import "ios/web/public/serializable_user_data_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Serialization keys used in NSCoding functions.
NSString* const kCertificatePolicyManagerKey = @"certificatePolicyManager";
NSString* const kCurrentNavigationIndexKey = @"currentNavigationIndex";
NSString* const kItemStoragesKey = @"entries";
NSString* const kOpenedByDOMKey = @"openedByDOM";
NSString* const kPreviousNavigationIndexKey = @"previousNavigationIndex";
}

@interface CRWSessionStorage () {
  // Backing object for property of same name.
  std::unique_ptr<web::SerializableUserData> _userData;
}

@end

@implementation CRWSessionStorage

@synthesize openedByDOM = _openedByDOM;
@synthesize currentNavigationIndex = _currentNavigationIndex;
@synthesize previousNavigationIndex = _previousNavigationIndex;
@synthesize itemStorages = _itemStorages;
@synthesize sessionCertificatePolicyManager = _sessionCertificatePolicyManager;

#pragma mark - Accessors

- (web::SerializableUserData*)userData {
  return _userData.get();
}

- (void)setSerializableUserData:
    (std::unique_ptr<web::SerializableUserData>)userData {
  _userData = std::move(userData);
}

#pragma mark - NSCoding

- (instancetype)initWithCoder:(nonnull NSCoder*)decoder {
  self = [super init];
  if (self) {
    _openedByDOM = [decoder decodeBoolForKey:kOpenedByDOMKey];
    _currentNavigationIndex =
        [decoder decodeIntForKey:kCurrentNavigationIndexKey];
    _previousNavigationIndex =
        [decoder decodeIntForKey:kPreviousNavigationIndexKey];
    _itemStorages = [[NSMutableArray alloc]
        initWithArray:[decoder decodeObjectForKey:kItemStoragesKey]];
    // Prior to M34, 0 was used as "no index" instead of -1; adjust for that.
    if (!_itemStorages.count)
      _currentNavigationIndex = -1;
    _sessionCertificatePolicyManager =
        [decoder decodeObjectForKey:kCertificatePolicyManagerKey];
    if (!_sessionCertificatePolicyManager) {
      _sessionCertificatePolicyManager =
          [[CRWSessionCertificatePolicyManager alloc] init];
    }
    _userData = web::SerializableUserData::Create();
    _userData->Decode(decoder);
  }
  return self;
}

- (void)encodeWithCoder:(NSCoder*)coder {
  [coder encodeBool:self.openedByDOM forKey:kOpenedByDOMKey];
  [coder encodeInt:self.currentNavigationIndex
            forKey:kCurrentNavigationIndexKey];
  [coder encodeInt:self.previousNavigationIndex
            forKey:kPreviousNavigationIndexKey];
  [coder encodeObject:self.itemStorages forKey:kItemStoragesKey];
  [coder encodeObject:self.sessionCertificatePolicyManager
               forKey:kCertificatePolicyManagerKey];
  if (_userData)
    _userData->Encode(coder);
}

@end
