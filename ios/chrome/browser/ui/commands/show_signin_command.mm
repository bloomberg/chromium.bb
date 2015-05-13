// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/commands/show_signin_command.h"

#include "base/logging.h"
#include "base/mac/scoped_block.h"
#include "ios/chrome/browser/ui/commands/ios_command_ids.h"

@implementation ShowSigninCommand {
  base::mac::ScopedBlock<ShowSigninCommandCompletionCallback> _callback;
}

@synthesize operation = _operation;
@synthesize signInSource = _signInSource;

- (instancetype)initWithTag:(NSInteger)tag {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithOperation:(AuthenticationOperation)operation
                     signInSource:(SignInSource)signInSource
                         callback:
                             (ShowSigninCommandCompletionCallback)callback {
  if ((self = [super initWithTag:IDC_SHOW_SIGNIN_IOS])) {
    _operation = operation;
    _signInSource = signInSource;
    _callback.reset(callback, base::scoped_policy::RETAIN);
  }
  return self;
}

- (instancetype)initWithOperation:(AuthenticationOperation)operation
                     signInSource:(SignInSource)signInSource {
  return
      [self initWithOperation:operation signInSource:signInSource callback:nil];
}

- (ShowSigninCommandCompletionCallback)callback {
  return _callback.get();
}

@end
