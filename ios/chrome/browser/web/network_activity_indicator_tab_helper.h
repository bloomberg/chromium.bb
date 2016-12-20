// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_NETWORK_ACTIVITY_INDICATOR_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_WEB_NETWORK_ACTIVITY_INDICATOR_TAB_HELPER_H_

#import <Foundation/Foundation.h>

#import "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "ios/web/public/web_state/web_state_observer.h"
#include "ios/web/public/web_state/web_state_user_data.h"

@class NSString;

// Handles listening to webState network activity to control network activity
// indicator.
class NetworkActivityIndicatorTabHelper
    : public web::WebStateObserver,
      public web::WebStateUserData<NetworkActivityIndicatorTabHelper> {
 public:
  static void CreateForWebState(web::WebState* web_state, NSString* tab_id);

 private:
  NetworkActivityIndicatorTabHelper(web::WebState* web_state, NSString* tab_id);
  ~NetworkActivityIndicatorTabHelper() override;

  // web::WebStateObserver overrides:
  void DidStartLoading() override;
  void DidStopLoading() override;

  // Clears any network activity state associated with this activity.
  void Stop();

  // Key used to uniquely identify this activity.
  base::scoped_nsobject<NSString> network_activity_key_;

  DISALLOW_COPY_AND_ASSIGN(NetworkActivityIndicatorTabHelper);
};

#endif  // IOS_CHROME_BROWSER_WEB_NETWORK_ACTIVITY_INDICATOR_TAB_HELPER_H_
