// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_state/web_state_delegate_bridge.h"

#include "base/logging.h"
#import "ios/web/public/web_state/context_menu_params.h"

namespace web {

WebStateDelegateBridge::WebStateDelegateBridge(id<CRWWebStateDelegate> delegate)
    : delegate_(delegate) {}

WebStateDelegateBridge::~WebStateDelegateBridge() {}

WebState* WebStateDelegateBridge::CreateNewWebState(WebState* source,
                                                    const GURL& url,
                                                    const GURL& opener_url,
                                                    bool initiated_by_user) {
  SEL selector =
      @selector(webState:createNewWebStateForURL:openerURL:initiatedByUser:);
  if ([delegate_ respondsToSelector:selector]) {
    return [delegate_ webState:source
        createNewWebStateForURL:url
                      openerURL:opener_url
                initiatedByUser:initiated_by_user];
  }
  return nullptr;
}

void WebStateDelegateBridge::CloseWebState(WebState* source) {
  if ([delegate_ respondsToSelector:@selector(closeWebState:)]) {
    [delegate_ closeWebState:source];
  }
}

WebState* WebStateDelegateBridge::OpenURLFromWebState(
    WebState* source,
    const WebState::OpenURLParams& params) {
  if ([delegate_ respondsToSelector:@selector(webState:openURLWithParams:)])
    return [delegate_ webState:source openURLWithParams:params];
  return nullptr;
}

bool WebStateDelegateBridge::HandleContextMenu(
    WebState* source,
    const ContextMenuParams& params) {
  if ([delegate_ respondsToSelector:@selector(webState:handleContextMenu:)]) {
    return [delegate_ webState:source handleContextMenu:params];
  }
  return NO;
}

void WebStateDelegateBridge::ShowRepostFormWarningDialog(
    WebState* source,
    const base::Callback<void(bool)>& callback) {
  base::Callback<void(bool)> local_callback(callback);
  SEL selector = @selector(webState:runRepostFormDialogWithCompletionHandler:);
  if ([delegate_ respondsToSelector:selector]) {
    [delegate_ webState:source
        runRepostFormDialogWithCompletionHandler:^(BOOL should_continue) {
          local_callback.Run(should_continue);
        }];
  } else {
    local_callback.Run(true);
  }
}

JavaScriptDialogPresenter* WebStateDelegateBridge::GetJavaScriptDialogPresenter(
    WebState* source) {
  SEL selector = @selector(javaScriptDialogPresenterForWebState:);
  if ([delegate_ respondsToSelector:selector]) {
    return [delegate_ javaScriptDialogPresenterForWebState:source];
  }
  return nullptr;
}

void WebStateDelegateBridge::OnAuthRequired(
    WebState* source,
    NSURLProtectionSpace* protection_space,
    NSURLCredential* proposed_credential,
    const AuthCallback& callback) {
  AuthCallback local_callback(callback);
  if ([delegate_
          respondsToSelector:@selector(webState:
                                 didRequestHTTPAuthForProtectionSpace:
                                                   proposedCredential:
                                                    completionHandler:)]) {
    [delegate_ webState:source
        didRequestHTTPAuthForProtectionSpace:protection_space
                          proposedCredential:proposed_credential
                           completionHandler:^(NSString* username,
                                               NSString* password) {
                             local_callback.Run(username, password);
                           }];
  } else {
    local_callback.Run(nil, nil);
  }
}

}  // web
