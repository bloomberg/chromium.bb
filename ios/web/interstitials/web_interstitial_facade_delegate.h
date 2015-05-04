// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_INTERSTITIALS_WEB_INTERSTITIAL_FACADE_DELEGATE_H_
#define IOS_WEB_INTERSTITIALS_WEB_INTERSTITIAL_FACADE_DELEGATE_H_

namespace content {
class InterstitialPage;
}

namespace web {

class WebInterstitialImpl;

// Interface used by WebInterstitials to drive their InterstitialPage facades.
class WebInterstitialFacadeDelegate {
 public:
  WebInterstitialFacadeDelegate() {}
  virtual ~WebInterstitialFacadeDelegate() {}

  // Sets the WebStateImpl that backs the WebContents facade.
  virtual void SetWebInterstitial(WebInterstitialImpl* web_interstitial) = 0;
  // Returns the facade object being driven by this delegate.
  virtual content::InterstitialPage* GetInterstitialPageFacade() = 0;

  // Called when the WebInterstitial is being destroyed.  Since WebInterstitials
  // manage their own deletion, this gives the delegate a chance to deallocate
  // the InterstitialPage facade.
  virtual void WebInterstitialDestroyed() = 0;
};

}  // namespace web

#endif  // IOS_WEB_INTERSTITIALS_WEB_INTERSTITIAL_FACADE_DELEGATE_H_
