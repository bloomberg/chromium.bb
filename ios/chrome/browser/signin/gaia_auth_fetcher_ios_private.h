// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_GAIA_AUTH_FETCHER_IOS_PRIVATE_H_
#define IOS_CHROME_BROWSER_SIGNIN_GAIA_AUTH_FETCHER_IOS_PRIVATE_H_

#import <Webkit/Webkit.h>

#import "base/mac/scoped_nsobject.h"
#include "base/macros.h"

class GaiaAuthFetcherIOS;
class GURL;

namespace web {
class BrowserState;
}

// Navigation delegate attached to a WKWebView used for URL fetches.
@interface GaiaAuthFetcherNavigationDelegate : NSObject<WKNavigationDelegate>
@end

// Bridge between the GaiaAuthFetcherIOS and the webview (and its navigation
// delegate) used to actually do the network fetch.
class GaiaAuthFetcherIOSBridge {
 public:
  GaiaAuthFetcherIOSBridge(GaiaAuthFetcherIOS* fetcher,
                           web::BrowserState* browser_state);
  ~GaiaAuthFetcherIOSBridge();

  // Starts a network fetch.
  // * |url| is the URL to fetch.
  // * |headers| are the HTTP headers to add to the request.
  // * |body| is the HTTP body to add to the request. If not empty, the fetch
  //   will be a POST request.
  void Fetch(const GURL& url,
             const std::string& headers,
             const std::string& body);

  // Cancels the current fetch.
  void Cancel();

  // Informs the bridge of the success of the URL fetch.
  // * |data| is the body of the HTTP response.
  // URLFetchSuccess and URLFetchFailure are no-op if one of them was already
  // called.
  void URLFetchSuccess(const std::string& data);

  // Informs the bridge of the failure of the URL fetch.
  // * |is_cancelled| whether the fetch failed because it was cancelled.
  // URLFetchSuccess and URLFetchFailure are no-op if one of them was already
  // called.
  void URLFetchFailure(bool is_cancelled);

 private:
  friend class GaiaAuthFetcherIOSTest;

  bool fetch_pending_;
  GaiaAuthFetcherIOS* fetcher_;
  GURL url_;
  base::scoped_nsobject<GaiaAuthFetcherNavigationDelegate> navigation_delegate_;
  base::scoped_nsobject<WKWebView> web_view_;

  DISALLOW_COPY_AND_ASSIGN(GaiaAuthFetcherIOSBridge);
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_GAIA_AUTH_FETCHER_IOS_PRIVATE_H_
