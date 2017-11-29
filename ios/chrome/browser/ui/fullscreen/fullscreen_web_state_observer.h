// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_WEB_STATE_OBSERVER_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_WEB_STATE_OBSERVER_H_

#include "ios/web/public/web_state/web_state_observer.h"

class FullscreenModel;

// A WebStateObserver that updates a FullscreenModel for navigation events.
class FullscreenWebStateObserver : public web::WebStateObserver {
 public:
  // Constructor for an observer that updates |model|.
  FullscreenWebStateObserver(FullscreenModel* model);

  // Tells the observer to start observing |web_state|.
  void SetWebState(web::WebState* web_state);

 private:
  // WebStateObserver:
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void DidStartLoading(web::WebState* web_state) override;
  void DidStopLoading(web::WebState* web_state) override;
  void DidChangeVisibleSecurityState(web::WebState* web_state) override;
  // Setter for whether the current page's SSL is broken.
  void SetIsSSLBroken(bool broken);
  // Setter for whether the WebState is currently loading.
  void SetIsLoading(bool loading);

  // The WebState being observed.
  web::WebState* web_state_ = nullptr;
  // The model passed on construction.
  FullscreenModel* model_;
  // Whether the page's SSL is broken.
  bool ssl_broken_ = false;
  // Whether the WebState is loading.
  bool loading_ = false;
};

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_WEB_STATE_OBSERVER_H_
