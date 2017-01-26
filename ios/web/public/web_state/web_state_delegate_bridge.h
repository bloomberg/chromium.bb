// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_STATE_WEB_STATE_DELEGATE_BRIDGE_H_
#define IOS_WEB_PUBLIC_WEB_STATE_WEB_STATE_DELEGATE_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "base/ios/weak_nsobject.h"
#import "ios/web/public/web_state/web_state_delegate.h"

// Objective-C interface for web::WebStateDelegate.
@protocol CRWWebStateDelegate<NSObject>
@optional

// Returns the WebState the URL is opened in, or nullptr if the URL wasn't
// opened immediately.
- (web::WebState*)webState:(web::WebState*)webState
         openURLWithParams:(const web::WebState::OpenURLParams&)params;

// Called when the user triggers the context menu with the given
// |ContextMenuParams|. Returns YES if the context menu operation was
// handled by the delegate. If this method is not implemented, the system
// context menu will be displayed.
- (BOOL)webState:(web::WebState*)webState
    handleContextMenu:(const web::ContextMenuParams&)params;

// Requests the repost form confirmation dialog. Clients must call |handler|
// with YES to allow repost and with NO to cancel the repost. If this method is
// not implemented then WebState will repost the form.
- (void)webState:(web::WebState*)webState
    runRepostFormDialogWithCompletionHandler:(void (^)(BOOL))handler;

// Returns a pointer to a service to manage dialogs. May return null in which
// case dialogs aren't shown.
- (web::JavaScriptDialogPresenter*)javaScriptDialogPresenterForWebState:
    (web::WebState*)webState;

// Called when a request receives an authentication challenge specified by
// |protectionSpace|, and is unable to respond using cached credentials.
// Clients must call |handler| even if they want to cancel authentication
// (in which case |username| or |password| should be nil).
- (void)webState:(web::WebState*)webState
    didRequestHTTPAuthForProtectionSpace:(NSURLProtectionSpace*)protectionSpace
                      proposedCredential:(NSURLCredential*)proposedCredential
                       completionHandler:(void (^)(NSString* username,
                                                   NSString* password))handler;
@end

namespace web {

// Adapter to use an id<CRWWebStateDelegate> as a web::WebStateDelegate.
class WebStateDelegateBridge : public web::WebStateDelegate {
 public:
  explicit WebStateDelegateBridge(id<CRWWebStateDelegate> delegate);
  ~WebStateDelegateBridge() override;

  // web::WebStateDelegate methods.
  WebState* OpenURLFromWebState(WebState*,
                                const WebState::OpenURLParams&) override;
  bool HandleContextMenu(WebState* source,
                         const ContextMenuParams& params) override;
  void ShowRepostFormWarningDialog(
      WebState* source,
      const base::Callback<void(bool)>& callback) override;
  JavaScriptDialogPresenter* GetJavaScriptDialogPresenter(
      WebState* source) override;
  void OnAuthRequired(WebState* source,
                      NSURLProtectionSpace* protection_space,
                      NSURLCredential* proposed_credential,
                      const AuthCallback& callback) override;

 private:
  // CRWWebStateDelegate which receives forwarded calls.
  base::WeakNSProtocol<id<CRWWebStateDelegate>> delegate_;
  DISALLOW_COPY_AND_ASSIGN(WebStateDelegateBridge);
};

}  // web

#endif  // IOS_WEB_PUBLIC_WEB_STATE_WEB_STATE_DELEGATE_BRIDGE_H_
