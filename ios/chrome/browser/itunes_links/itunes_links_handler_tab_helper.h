// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ITUNES_LINKS_ITUNES_LINKS_HANDLER_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_ITUNES_LINKS_ITUNES_LINKS_HANDLER_TAB_HELPER_H_

#include "base/macros.h"
#include "ios/web/public/web_state/web_state_observer.h"
#include "ios/web/public/web_state/web_state_user_data.h"

namespace web {
class WebStatePolicyDecider;
}

// Enum for the IOS.StoreKit.ITunesURLsHandlingResult UMA histogram to report
// the results of the StoreKit handling.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ITunesUrlsStoreKitHandlingResult {
  // ITunes URL handling failed. This can happen if the Storekit tab helper is
  // null, which in general should not happen, but if it happens StoreKit will
  // not launch and this will value be logged.
  kUrlHandlingFailed = 0,
  // ITunes URL for single app was handled by the StoreKit.
  kSingleAppUrlHandled = 1,
  // ITunes URL for app bundle was handled by the StoreKit.
  kBundleUrlHandled = 2,
  // ITunes URL for app bundle was not handled by the StoreKit.
  kBundleUrlNotHandled = 3,
  kCount
};

// TabHelper which handles navigation to iTunes links.
// If a navigation to web page for a product in iTunes App Store happens while
// in non off the record browsing mode, this helper will use StoreKitTabHelper
// to present the information of that product. The goal of this class is to
// workaround a bug where appstore website serves the wrong content for
// itunes.apple.com pages, see http://crbug.com/623016.
class ITunesLinksHandlerTabHelper
    : public web::WebStateObserver,
      public web::WebStateUserData<ITunesLinksHandlerTabHelper> {
 public:
  ~ITunesLinksHandlerTabHelper() override;
  explicit ITunesLinksHandlerTabHelper(web::WebState* web_state);

 private:
  // web::WebStateObserver implementation
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // PolicyDecider instance that will be initialized with the
  // ITunesLinkHandlerTabHelper object, and destroyed with it.
  std::unique_ptr<web::WebStatePolicyDecider> policy_decider_;

  DISALLOW_COPY_AND_ASSIGN(ITunesLinksHandlerTabHelper);
};

#endif  // IOS_CHROME_BROWSER_ITUNES_LINKS_ITUNES_LINKS_HANDLER_TAB_HELPER_H_
