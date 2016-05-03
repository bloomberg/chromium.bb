// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/web_state_delegate_stub.h"

#import "ios/web/public/web_state/web_state.h"
#import "ios/web/public/web_state/context_menu_params.h"

@implementation CRWWebStateDelegateStub {
  // Backs up the property with the same name.
  std::unique_ptr<web::ContextMenuParams> _contextMenuParams;
}

@synthesize webState = _webState;
@synthesize changedProgress = _changedProgress;

- (void)webState:(web::WebState*)webState didChangeProgress:(double)progress {
  _webState = webState;
  _changedProgress = progress;
}

- (BOOL)webState:(web::WebState*)webState
    handleContextMenu:(const web::ContextMenuParams&)params {
  _webState = webState;
  _contextMenuParams.reset(new web::ContextMenuParams(params));
  return YES;
}

- (web::ContextMenuParams*)contextMenuParams {
  return _contextMenuParams.get();
}

@end
