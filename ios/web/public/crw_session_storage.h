// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_CRW_SESSION_STORAGE_H_
#define IOS_WEB_PUBLIC_CRW_SESSION_STORAGE_H_

#import <Foundation/Foundation.h>
#include <memory>

@class CRWSessionCertificatePolicyManager;

namespace web {
class SerializableUserData;
}

// NSCoding-compliant class used to serialize session state.
// TODO(crbug.com/685388): Investigate using code from the sessions component.
@interface CRWSessionStorage : NSObject<NSCoding>

@property(nonatomic, getter=isOpenedByDOM) BOOL openedByDOM;
@property(nonatomic, assign) NSInteger currentNavigationIndex;
@property(nonatomic, assign) NSInteger previousNavigationIndex;
@property(nonatomic, copy) NSArray* itemStorages;
@property(nonatomic, retain)
    CRWSessionCertificatePolicyManager* sessionCertificatePolicyManager;
@property(nonatomic, readonly) web::SerializableUserData* userData;

// Setter for |userData|.  The receiver takes ownership of |userData|.
- (void)setSerializableUserData:
    (std::unique_ptr<web::SerializableUserData>)userData;

@end

#endif  // IOS_WEB_PUBLIC_CRW_SESSION_STORAGE_H_
