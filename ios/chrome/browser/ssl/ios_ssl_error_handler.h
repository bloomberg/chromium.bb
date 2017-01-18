// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SSL_IOS_SSL_ERROR_HANDLER_H_
#define IOS_CHROME_BROWSER_SSL_IOS_SSL_ERROR_HANDLER_H_

#include "base/callback_forward.h"
#include "url/gurl.h"

namespace net {
class SSLInfo;
}  // namespace net

namespace web {
class WebState;
}  // namespace web

// This class is responsible for deciding what type of interstitial to show for
// an SSL validation error.
class IOSSSLErrorHandler {
 public:
  // Entry point for the class.
  static void HandleSSLError(web::WebState* web_state,
                             int cert_error,
                             const net::SSLInfo& info,
                             const GURL& request_url,
                             bool overridable,
                             const base::Callback<void(bool)>& callback);

 private:
  ~IOSSSLErrorHandler() = delete;
  // Called on SSL interstitial dismissal.
  static void InterstitialWasDismissed(
      web::WebState* web_state,
      const base::Callback<void(bool)>& callback,
      bool proceed);
  // Records a metric to classify if SSL errors are due to a Captive Portal
  // state.
  static void RecordCaptivePortalState(web::WebState* web_state);
  DISALLOW_IMPLICIT_CONSTRUCTORS(IOSSSLErrorHandler);
};

#endif  // IOS_CHROME_BROWSER_SSL_IOS_SSL_ERROR_HANDLER_H_
