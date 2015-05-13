// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/commands/clear_browsing_data_command.h"

#include "base/logging.h"
#include "ios/chrome/browser/ui/commands/ios_command_ids.h"

@implementation ClearBrowsingDataCommand

@synthesize browserState = _browserState;
@synthesize mask = _mask;

- (instancetype)initWithTag:(NSInteger)tag {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                                mask:(int)mask {
  self = [super initWithTag:IDC_CLEAR_BROWSING_DATA_IOS];
  if (self) {
    DCHECK(browserState);
    _browserState = browserState;
    _mask = mask;
  }
  return self;
}

@end
