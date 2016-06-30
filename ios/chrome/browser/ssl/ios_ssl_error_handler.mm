// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ssl/ios_ssl_error_handler.h"

#include "base/bind.h"
#include "base/callback.h"
#include "components/security_interstitials/core/ssl_error_ui.h"
#include "ios/chrome/browser/ssl/ios_ssl_blocking_page.h"
#import "ios/web/public/navigation_manager.h"
#import "ios/web/public/web_state/web_state.h"
#include "net/ssl/ssl_info.h"

// static
void IOSSSLErrorHandler::HandleSSLError(
    web::WebState* web_state,
    int cert_error,
    const net::SSLInfo& info,
    const GURL& request_url,
    bool overridable,
    const base::Callback<void(bool)>& callback) {
  DCHECK(!web_state->IsShowingWebInterstitial());

  int options_mask =
      overridable ? security_interstitials::SSLErrorUI::SOFT_OVERRIDE_ENABLED
                  : security_interstitials::SSLErrorUI::STRICT_ENFORCEMENT;
  // SSLBlockingPage deletes itself when it's dismissed.
  auto dismissal_callback(
      base::Bind(&IOSSSLErrorHandler::InterstitialWasDismissed,
                 base::Unretained(web_state), callback));
  IOSSSLBlockingPage* page = new IOSSSLBlockingPage(
      web_state, cert_error, info, request_url, options_mask,
      base::Time::NowFromSystemTime(), dismissal_callback);
  page->Show();
}

// static
void IOSSSLErrorHandler::InterstitialWasDismissed(
    web::WebState* web_state,
    const base::Callback<void(bool)>& callback,
    bool proceed) {
  if (!proceed) {
    web_state->GetNavigationManager()->Reload(true /* check_for_repost */);
  }
  callback.Run(proceed);
}
