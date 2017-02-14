// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_WEBSITE_DATA_STORE_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_WEBSITE_DATA_STORE_H_

#import <Foundation/Foundation.h>

// Controls persistence of website data such as cookies, caches, and local
// storage.
@interface CWVWebsiteDataStore : NSObject

// Whether or not this data store persists data to disk.
@property(readonly, getter=isPersistent) BOOL persistent;

// Persistent data store which stores all data on disk.
+ (instancetype)defaultDataStore;
// Ephemeral data store that never stores data on disk.
+ (instancetype)nonPersistentDataStore;

@end

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_WEBSITE_DATA_STORE_H_
