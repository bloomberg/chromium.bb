// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_WEB_STATE_DELEGATE_STUB_H_
#define IOS_WEB_WEB_STATE_WEB_STATE_DELEGATE_STUB_H_

#import "ios/testing/ocmock_complex_type_helper.h"
#import "ios/web/public/web_state/web_state_delegate_bridge.h"

// Stub implementation for CRWWebStateDelegate protocol.
@interface CRWWebStateDelegateStub
    : OCMockComplexTypeHelper<CRWWebStateDelegate>
// web::WebState received in delegate method calls..
@property(nonatomic, readonly) web::WebState* webState;
// Progress received in |webState:didChangeProgress| call.
@property(nonatomic, readonly) double changedProgress;
// ContextMenuParams reveived in |webState:handleContextMenu:| call.
// nullptr if that delegate method was not called.
@property(nonatomic, readonly) web::ContextMenuParams* contextMenuParams;
@end

#endif  // IOS_WEB_WEB_STATE_WEB_STATE_DELEGATE_STUB_H_
