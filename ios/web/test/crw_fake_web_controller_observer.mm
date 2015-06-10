// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/test/crw_fake_web_controller_observer.h"

#import "base/logging.h"
#import "base/mac/scoped_nsobject.h"
#include "base/values.h"

@implementation CRWFakeWebControllerObserver {
  ScopedVector<base::DictionaryValue> _commandsReceived;
  base::scoped_nsobject<NSString> _commandPrefix;
}

@synthesize pageLoaded = _pageLoaded;

- (instancetype)initWithCommandPrefix:(NSString*)commandPrefix {
  DCHECK(commandPrefix);
  self = [super init];
  if (self) {
    _commandPrefix.reset([commandPrefix copy]);
  }
  return self;
}

- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (void)pageLoaded:(CRWWebController*)webController {
  _pageLoaded = YES;
}

- (NSString*)commandPrefix {
  return _commandPrefix;
}

- (BOOL)handleCommand:(const base::DictionaryValue&)command
        webController:(CRWWebController*)webController
    userIsInteracting:(BOOL)userIsInteracting
            originURL:(const GURL&)originURL {
  _commandsReceived.push_back(command.DeepCopy());
  return YES;
}

- (ScopedVector<base::DictionaryValue>&)commandsReceived {
  return _commandsReceived;
}

@end
