// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/itunes_links/itunes_links_handler_tab_helper.h"

#import <Foundation/Foundation.h>

#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/store_kit/store_kit_tab_helper.h"
#import "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"
#import "ios/web/public/web_state/navigation_context.h"
#import "ios/web/public/web_state/web_state_policy_decider.h"
#import "net/base/mac/url_conversions.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

DEFINE_WEB_STATE_USER_DATA_KEY(ITunesLinksHandlerTabHelper);

namespace {

// The domain for iTunes appstore links.
const char kITunesUrlDomain[] = "itunes.apple.com";
const char kITunesProductIdPrefix[] = "id";

// Returns true, it the given |url| is iTunes product url.
bool IsITunesProductUrl(const GURL& url) {
  if (!url.SchemeIsHTTPOrHTTPS() || !url.DomainIs(kITunesUrlDomain))
    return false;
  std::string file_name = url.ExtractFileName();
  // The first |kITunesProductIdLength| characters must be
  // |kITunesProductIdPrefix|, followed by the app ID.
  size_t prefix_length = strlen(kITunesProductIdPrefix);
  return (file_name.length() > prefix_length &&
          file_name.substr(0, prefix_length) == kITunesProductIdPrefix);
}

// This class handles requests & responses that involve iTunes product links.
class ITunesLinksHandlerWebStatePolicyDecider
    : public web::WebStatePolicyDecider {
 public:
  explicit ITunesLinksHandlerWebStatePolicyDecider(web::WebState* web_state)
      : web::WebStatePolicyDecider(web_state) {}

  // web::WebStatePolicyDecider implementation
  bool ShouldAllowResponse(NSURLResponse* response,
                           bool for_main_frame) override {
    // Don't allow rendering responses from Itunes appstore URLs unless it's on
    // iframe.
    return !for_main_frame ||
           !IsITunesProductUrl(net::GURLWithNSURL(response.URL));
  }

  bool ShouldAllowRequest(NSURLRequest* request,
                          ui::PageTransition transition) override {
    web::NavigationItem* pending_item =
        web_state()->GetNavigationManager()->GetPendingItem();

    if (!pending_item)
      return true;
    // If the pending item URL is http iTunes appstore URL, but the request URL
    // is not http URL, then there was a redirect to an external application and
    // request should be blocked to be able to show the store kit later.
    GURL pending_item_url = pending_item->GetURL();
    GURL request_url = net::GURLWithNSURL(request.URL);
    return !IsITunesProductUrl(pending_item_url) ||
           request_url.SchemeIsHTTPOrHTTPS();
  }
};

}  // namespace

ITunesLinksHandlerTabHelper::~ITunesLinksHandlerTabHelper() = default;

ITunesLinksHandlerTabHelper::ITunesLinksHandlerTabHelper(
    web::WebState* web_state)
    : policy_decider_(std::make_unique<ITunesLinksHandlerWebStatePolicyDecider>(
          web_state)) {
  web_state->AddObserver(this);
}

// WebStateObserver
void ITunesLinksHandlerTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  GURL url = navigation_context->GetUrl();
  // Whenever a navigation to iTunes product url is finished, launch StoreKit.
  if (IsITunesProductUrl(url)) {
    // If the url is iTunes product url, then this navigation should not be
    // committed, as the policy decider's ShouldAllowResponse will return false.
    DCHECK(!navigation_context->HasCommitted());
    std::string product_id =
        url.ExtractFileName().substr(strlen(kITunesProductIdPrefix));
    StoreKitTabHelper* tab_helper = StoreKitTabHelper::FromWebState(web_state);
    if (tab_helper) {
      base::RecordAction(
          base::UserMetricsAction("ITunesLinksHandler_StoreKitLaunched"));
      tab_helper->OpenAppStore(base::SysUTF8ToNSString(product_id));
    }
  }
}

void ITunesLinksHandlerTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state->RemoveObserver(this);
}
