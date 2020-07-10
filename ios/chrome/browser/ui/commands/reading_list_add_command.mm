// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/commands/reading_list_add_command.h"

#include "base/logging.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ReadingListAddCommand {
  GURL _URL;
}

@synthesize title = _title;
@synthesize URL = _URL;

- (instancetype)initWithURL:(const GURL&)URL title:(NSString*)title {
  if (self = [super init]) {
    _URL = URL;
    _title = title;
  }
  return self;
}

@end
