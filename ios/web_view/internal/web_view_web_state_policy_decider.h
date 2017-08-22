// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_WEB_VIEW_WEB_STATE_POLICY_DECIDER_H_
#define IOS_WEB_VIEW_INTERNAL_WEB_VIEW_WEB_STATE_POLICY_DECIDER_H_

#import <Foundation/Foundation.h>

#import "ios/web/public/web_state/web_state_policy_decider.h"

namespace web {
class WebState;
}

@class CWVWebView;

namespace ios_web_view {

// An implementation of web::WebStatePolicyDecider which delegates to:
//   [web_view.navigationDelegate webView:shouldStartLoadWithRequest:]
//   [web_view.navigationDelegate webView:shouldContinueLoadWithResponse:]
class WebViewWebStatePolicyDecider : public web::WebStatePolicyDecider {
 public:
  WebViewWebStatePolicyDecider(web::WebState* web_state, CWVWebView* web_view);

  // web::WebStatePolicyDecider overrides:
  bool ShouldAllowRequest(NSURLRequest* request,
                          ui::PageTransition transition) override;
  bool ShouldAllowResponse(NSURLResponse* response) override;

 private:
  // Delegates to |delegate| property of this web view.
  __unsafe_unretained CWVWebView* web_view_;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_WEB_VIEW_WEB_STATE_POLICY_DECIDER_H_
