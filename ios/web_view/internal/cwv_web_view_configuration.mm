// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/public/cwv_web_view_configuration.h"

#import "ios/web_view/public/cwv_website_data_store.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CWVWebViewConfiguration ()
// Initialize configuration with specified data store.
- (instancetype)initWithDataStore:(CWVWebsiteDataStore*)dataStore;
@end

@implementation CWVWebViewConfiguration

@synthesize websiteDataStore = _websiteDataStore;

- (instancetype)init {
  return [self initWithDataStore:[CWVWebsiteDataStore defaultDataStore]];
}

- (instancetype)initWithDataStore:(CWVWebsiteDataStore*)dataStore {
  self = [super init];
  if (self) {
    _websiteDataStore = dataStore;
  }
  return self;
}

// NSCopying

- (id)copyWithZone:(NSZone*)zone {
  return
      [[[self class] allocWithZone:zone] initWithDataStore:_websiteDataStore];
}

@end
