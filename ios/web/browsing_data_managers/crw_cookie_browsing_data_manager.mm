// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/browsing_data_managers/crw_cookie_browsing_data_manager.h"

#import "base/logging.h"
#include "ios/web/public/browser_state.h"

@implementation CRWCookieBrowsingDataManager {
  web::BrowserState* _browserState;  // Weak, owns this object.
}

- (instancetype)initWithBrowserState:(web::BrowserState*)browserState {
  self = [super init];
  if (self) {
    DCHECK(browserState);
    _browserState = browserState;
  }
  return self;
}

- (instancetype)init {
  NOTREACHED();
  return nil;
}

#pragma mark CRWBrowsingDataManager implementation
// TODO(shreyasv): During implementation of the following methods evaluate if
// the entire BrowserState is required. Looks like only |state_path| may be
// required.

- (void)stashData {
  // TODO(shreyasv): Implement this. crbug.com/480654
}

- (void)restoreData {
  // TODO(shreyasv): Implement this. crbug.com/480654
}

- (void)removeDataAtStashPath {
  // TODO(shreyasv): Implement this. crbug.com/480654
}

- (void)removeDataAtCanonicalPath {
  // TODO(shreyasv): Implement this. crbug.com/480654
}

@end
