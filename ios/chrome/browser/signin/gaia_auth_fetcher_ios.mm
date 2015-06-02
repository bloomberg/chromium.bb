// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/signin/gaia_auth_fetcher_ios.h"

#include "base/logging.h"
#include "ios/chrome/browser/experimental_flags.h"
#include "net/base/load_flags.h"

GaiaAuthFetcherIOS::GaiaAuthFetcherIOS(GaiaAuthConsumer* consumer,
                                       const std::string& source,
                                       net::URLRequestContextGetter* getter,
                                       web::BrowserState* browser_state)
    : GaiaAuthFetcher(consumer, source, getter) {
}

GaiaAuthFetcherIOS::~GaiaAuthFetcherIOS() {
}

void GaiaAuthFetcherIOS::CreateAndStartGaiaFetcher(const std::string& body,
                                                   const std::string& headers,
                                                   const GURL& gaia_gurl,
                                                   int load_flags) {
  if (!experimental_flags::IsWKWebViewEnabled() ||
      !(load_flags &
        (net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SAVE_COOKIES))) {
    GaiaAuthFetcher::CreateAndStartGaiaFetcher(body, headers, gaia_gurl,
                                               load_flags);
    return;
  }
  // TODO(bzanotti): Fetch |gaia_url| using a WKWebView. crbug.com/495597
  NOTIMPLEMENTED();
}
