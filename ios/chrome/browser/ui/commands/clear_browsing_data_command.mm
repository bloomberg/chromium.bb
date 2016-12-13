// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/commands/clear_browsing_data_command.h"

#include "base/logging.h"
#include "ios/chrome/browser/ui/commands/ios_command_ids.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ClearBrowsingDataCommand

@synthesize browserState = _browserState;
@synthesize mask = _mask;
@synthesize timePeriod = _timePeriod;

- (instancetype)initWithTag:(NSInteger)tag {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                                mask:(int)mask
                          timePeriod:(browsing_data::TimePeriod)timePeriod {
  self = [super initWithTag:IDC_CLEAR_BROWSING_DATA_IOS];
  if (self) {
    DCHECK(browserState);
    _browserState = browserState;
    _mask = mask;
    _timePeriod = timePeriod;
  }
  return self;
}

@end
