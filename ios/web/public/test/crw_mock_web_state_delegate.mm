// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/crw_mock_web_state_delegate.h"

#import "ios/web/public/web_state/context_menu_params.h"
#import "ios/web/public/web_state/web_state.h"

@implementation CRWMockWebStateDelegate {
  // Backs up the property with the same name.
  std::unique_ptr<web::WebState::OpenURLParams> _openURLParams;
  // Backs up the property with the same name.
  std::unique_ptr<web::ContextMenuParams> _contextMenuParams;
  // Backs up the property with the same name.
  BOOL _javaScriptDialogPresenterRequested;
}

@synthesize webState = _webState;
@synthesize repostFormWarningRequested = _repostFormWarningRequested;
@synthesize authenticationRequested = _authenticationRequested;

- (web::WebState*)webState:(web::WebState*)webState
         openURLWithParams:(const web::WebState::OpenURLParams&)params {
  _webState = webState;
  _openURLParams.reset(new web::WebState::OpenURLParams(params));
  return webState;
}

- (BOOL)webState:(web::WebState*)webState
    handleContextMenu:(const web::ContextMenuParams&)params {
  _webState = webState;
  _contextMenuParams.reset(new web::ContextMenuParams(params));
  return YES;
}

- (void)webState:(web::WebState*)webState
    runRepostFormDialogWithCompletionHandler:(void (^)(BOOL))handler {
  _webState = webState;
  _repostFormWarningRequested = YES;
}

- (web::JavaScriptDialogPresenter*)javaScriptDialogPresenterForWebState:
    (web::WebState*)webState {
  _webState = webState;
  _javaScriptDialogPresenterRequested = YES;
  return nil;
}

- (void)webState:(web::WebState*)webState
    didRequestHTTPAuthForProtectionSpace:(NSURLProtectionSpace*)protectionSpace
                      proposedCredential:(NSURLCredential*)proposedCredential
                       completionHandler:(void (^)(NSString* username,
                                                   NSString* password))handler {
  _webState = webState;
  _authenticationRequested = YES;
}

- (const web::WebState::OpenURLParams*)openURLParams {
  return _openURLParams.get();
}

- (web::ContextMenuParams*)contextMenuParams {
  return _contextMenuParams.get();
}

- (BOOL)javaScriptDialogPresenterRequested {
  return _javaScriptDialogPresenterRequested;
}

@end
