// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_INTERSTITIALS_HTML_WEB_INTERSTITIAL_IMPL_H_
#define IOS_WEB_INTERSTITIALS_HTML_WEB_INTERSTITIAL_IMPL_H_

#import <UIKit/UIKit.h>

#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "ios/web/interstitials/web_interstitial_impl.h"

@protocol CRWSimpleWebViewController;
@protocol CRWSimpleWebViewControllerDelegate;

namespace web {

class HtmlWebInterstitialDelegate;
class HtmlWebInterstitialImpl;

// A concrete subclass of WebInterstitialImpl that is used to display
// interstitials created via HTML.
class HtmlWebInterstitialImpl : public WebInterstitialImpl {
 public:
  HtmlWebInterstitialImpl(WebStateImpl* web_state,
                          bool new_navigation,
                          const GURL& url,
                          scoped_ptr<HtmlWebInterstitialDelegate> delegate);
  ~HtmlWebInterstitialImpl() override;

  // Called by |web_view_controller_delegate_| when |web_view_controller_|
  // receives a JavaScript command.
  void CommandReceivedFromWebView(NSString* command);

  // WebInterstitialImpl implementation:
  CRWContentView* GetContentView() const override;

 protected:
  // WebInterstitialImpl implementation:
  void PrepareForDisplay() override;
  WebInterstitialDelegate* GetDelegate() const override;
  void EvaluateJavaScript(NSString* script,
                          JavaScriptCompletion completionHandler) override;

 private:
  // The HTML interstitial delegate.
  scoped_ptr<HtmlWebInterstitialDelegate> delegate_;
  // The |web_view_controller_|'s delegate.  Used to forward JavaScript commands
  // resulting from user interaction with the interstitial content.
  base::scoped_nsprotocol<id<CRWSimpleWebViewControllerDelegate>>
      web_view_controller_delegate_;
  // The CRWSimpleWebViewController that contains the web view used to show the
  // content. View needs to be resized by the caller.
  base::scoped_nsprotocol<id<CRWSimpleWebViewController>> web_view_controller_;
  // The CRWContentView used to display |web_view_controller_|'s view.
  base::scoped_nsobject<CRWContentView> content_view_;
};

}  // namespace web

#endif  // IOS_WEB_INTERSTITIALS_HTML_WEB_INTERSTITIAL_IMPL_H_
