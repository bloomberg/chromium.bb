// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SSL_IOS_SSL_ERROR_HANDLER_H_
#define IOS_CHROME_BROWSER_SSL_IOS_SSL_ERROR_HANDLER_H_

#include "base/callback_forward.h"
#include "components/captive_portal/captive_portal_types.h"
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
  // Displays an SSL error page interstitial for the given |request_url| and
  // |web_state|. The |cert_error| and SSL |info| represent the SSL error
  // detected which triggered the display of the SSL interstitial. |callback|
  // will be called after the user is done interacting with this interstitial.
  static void ShowSSLInterstitial(web::WebState* web_state,
                                  int cert_error,
                                  const net::SSLInfo& info,
                                  const GURL& request_url,
                                  bool overridable,
                                  const base::Callback<void(bool)>& callback);
  // Displays a Captive Portal interstitial for the given |request_url| and
  // |web_state|. The |landing_url| is the web page which allows the user to
  // complete their connection to the network. |callback| will be called after
  // the user is done interacting with this interstitial.
  static void ShowCaptivePortalInterstitial(
      web::WebState* web_state,
      const GURL& request_url,
      const GURL& landing_url,
      const base::Callback<void(bool)>& callback);
  // Detects the current Captive Portal state and records the result with
  // |LogCaptivePortalResult|.
  static void RecordCaptivePortalState(web::WebState* web_state);
  // Records a metric to classify if SSL errors are due to a Captive Portal
  // state.
  static void LogCaptivePortalResult(
      captive_portal::CaptivePortalResult result);
  // Called on SSL interstitial dismissal.
  static void InterstitialWasDismissed(
      web::WebState* web_state,
      const base::Callback<void(bool)>& callback,
      bool proceed);
  DISALLOW_IMPLICIT_CONSTRUCTORS(IOSSSLErrorHandler);
};

#endif  // IOS_CHROME_BROWSER_SSL_IOS_SSL_ERROR_HANDLER_H_
