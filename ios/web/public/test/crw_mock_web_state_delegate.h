// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_CRW_MOCK_WEB_STATE_DELEGATE_H_
#define IOS_WEB_PUBLIC_TEST_CRW_MOCK_WEB_STATE_DELEGATE_H_

#import "ios/testing/ocmock_complex_type_helper.h"
#import "ios/web/public/web_state/web_state_delegate_bridge.h"

// Stub implementation for CRWWebStateDelegate protocol.
@interface CRWMockWebStateDelegate
    : OCMockComplexTypeHelper<CRWWebStateDelegate>
// web::WebState::OpenURLParams in |webState:openURLWithParams:| call.
@property(nonatomic, readonly)
    const web::WebState::OpenURLParams* openURLParams;
// web::WebState received in delegate method calls.
@property(nonatomic, readonly) web::WebState* webState;
// ContextMenuParams reveived in |webState:handleContextMenu:| call.
// nullptr if that delegate method was not called.
@property(nonatomic, readonly) web::ContextMenuParams* contextMenuParams;
// Whether |webState:runRepostFormDialogWithCompletionHandler:| has been called
// or not.
@property(nonatomic, readonly) BOOL repostFormWarningRequested;
// Whether |javaScriptDialogPresenterForWebState:| has been called or not.
@property(nonatomic, readonly) BOOL javaScriptDialogPresenterRequested;
// Whether |webState:didRequestHTTPAuthForProtectionSpace:...| has been called
// or not.
@property(nonatomic, readonly) BOOL authenticationRequested;

@end

#endif  // IOS_WEB_PUBLIC_TEST_CRW_MOCK_WEB_STATE_DELEGATE_H_
