// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/web_state_delegate_stub.h"

#import "ios/web/public/web_state/web_state.h"

@implementation CRWWebStateDelegateStub

@synthesize webState = _webState;
@synthesize changedProgress = _changedProgress;

- (void)webState:(web::WebState*)webState didChangeProgress:(double)progress {
  _webState = webState;
  _changedProgress = progress;
}

@end
