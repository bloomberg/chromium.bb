// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_CRW_NAVIGATION_MANAGER_STORAGE_H_
#define IOS_WEB_PUBLIC_CRW_NAVIGATION_MANAGER_STORAGE_H_

#import <Foundation/Foundation.h>

@class CRWSessionCertificatePolicyManager;

// NSCoding-compliant class used to serialize NavigationManager's persisted
// properties.
// TODO(crbug.com/685388): Investigate using code from the sessions component.
@interface CRWNavigationManagerStorage : NSObject<NSCoding>

@property(nonatomic, copy) NSString* tabID;
@property(nonatomic, copy) NSString* openerID;
@property(nonatomic, getter=isOpenedByDOM) BOOL openedByDOM;
@property(nonatomic, assign) NSInteger openerNavigationIndex;
@property(nonatomic, copy) NSString* windowName;
@property(nonatomic, assign) NSInteger currentNavigationIndex;
@property(nonatomic, assign) NSInteger previousNavigationIndex;
@property(nonatomic, assign) NSTimeInterval lastVisitedTimestamp;
// TODO(crbug.com/454984): |entries| currently contains CRWSessionEntries.  Once
// NavigationItemImpls are encoded/decoded using CRWNavigationItemSerializations
// (analogous to the approach taken with NavigationManager), this will
// become an array of CRWNavigationItemSerializations.
@property(nonatomic, copy) NSArray* entries;
@property(nonatomic, retain)
    CRWSessionCertificatePolicyManager* sessionCertificatePolicyManager;

@end

#endif  // IOS_WEB_PUBLIC_CRW_NAVIGATION_MANAGER_STORAGE_H_
