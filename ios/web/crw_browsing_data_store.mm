// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/crw_browsing_data_store.h"

#include "base/logging.h"

@implementation CRWBrowsingDataStore {
  web::BrowserState* _browserState;  // Weak, owns this object.
}

- (instancetype)initWithBrowserState:(web::BrowserState*)browserState {
  self = [super init];
  if (self) {
    DCHECK(browserState);
    // TODO(shreyasv): Instantiate the necessary CRWBrowsingDataManagers that
    // are encapsulated within this class. crbug.com/480654.
    _browserState = browserState;
  }
  return self;
}

@end
