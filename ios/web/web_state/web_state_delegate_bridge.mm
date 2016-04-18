// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_state/web_state_delegate_bridge.h"

#include "base/logging.h"

namespace web {

WebStateDelegateBridge::WebStateDelegateBridge(id<CRWWebStateDelegate> delegate)
    : delegate_(delegate) {}

WebStateDelegateBridge::~WebStateDelegateBridge() {}

void WebStateDelegateBridge::LoadProgressChanged(WebState* source,
                                                 double progress) {
  if ([delegate_ respondsToSelector:@selector(webState:didChangeProgress:)])
    [delegate_ webState:source didChangeProgress:progress];
}

}  // web