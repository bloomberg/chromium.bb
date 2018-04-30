// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/itunes_links/itunes_links_handler_tab_helper.h"

#import <Foundation/Foundation.h>
#import <StoreKit/StoreKit.h>

#include <vector>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/store_kit/store_kit_tab_helper.h"
#include "ios/web/public/browser_state.h"
#import "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"
#import "ios/web/public/web_state/navigation_context.h"
#import "ios/web/public/web_state/web_state_policy_decider.h"
#include "net/base/filename_util.h"
#import "net/base/mac/url_conversions.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

DEFINE_WEB_STATE_USER_DATA_KEY(ITunesLinksHandlerTabHelper);

namespace {

// The domain for iTunes appstore links.
const char kITunesUrlDomain[] = "itunes.apple.com";
const char kITunesProductIdPrefix[] = "id";
const char kITunesAppPathIdentifier[] = "app";
const size_t kITunesUrlPathMinComponentsCount = 4;
const size_t kITunesUrlRegionComponentDefaultIndex = 1;
const size_t kITunesUrlMediaTypeComponentDefaultIndex = 2;

// Records the StoreKit handling result to IOS.StoreKit.ITunesURLsHandlingResult
// UMA histogram.
void RecordStoreKitHandlingResult(ITunesUrlsStoreKitHandlingResult result) {
  UMA_HISTOGRAM_ENUMERATION("IOS.StoreKit.ITunesURLsHandlingResult", result,
                            ITunesUrlsStoreKitHandlingResult::kCount);
}

// Returns true, it the given |url| is iTunes product URL.
// iTunes URL should start with apple domain and has product id.
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

// Extracts iTunes product parameters from the given |url| to be used with the
// StoreKit launcher.
NSDictionary* ExtractITunesProductParameters(const GURL& url) {
  NSMutableDictionary<NSString*, NSString*>* params_dictionary =
      [[NSMutableDictionary alloc] init];
  std::string product_id =
      url.ExtractFileName().substr(strlen(kITunesProductIdPrefix));
  params_dictionary[SKStoreProductParameterITunesItemIdentifier] =
      base::SysUTF8ToNSString(product_id);
  for (net::QueryIterator it(url); !it.IsAtEnd(); it.Advance()) {
    params_dictionary[base::SysUTF8ToNSString(it.GetKey())] =
        base::SysUTF8ToNSString(it.GetValue());
  }
  return params_dictionary;
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
    // Don't allow rendering responses from URLs that can be handled by
    // iTunesLinksHandler unless it's on iframe or the browsing mode is off the
    // record.
    return web_state()->GetBrowserState()->IsOffTheRecord() ||
           !for_main_frame || !CanHandleUrl(net::GURLWithNSURL(response.URL));
  }

  bool ShouldAllowRequest(NSURLRequest* request,
                          ui::PageTransition transition) override {
    // Only consider blocking the request if it's not of the record mode.
    if (web_state()->GetBrowserState()->IsOffTheRecord())
      return true;
    web::NavigationItem* pending_item =
        web_state()->GetNavigationManager()->GetPendingItem();

    if (!pending_item)
      return true;
    // If the pending item URL is http iTunes URL that can be handled, but the
    // request URL is not http URL, then there was a redirect to an external
    // application and request should be blocked to be able to show the store
    // kit later.
    GURL pending_item_url = pending_item->GetURL();
    GURL request_url = net::GURLWithNSURL(request.URL);
    return !CanHandleUrl(pending_item_url) || request_url.SchemeIsHTTPOrHTTPS();
  }

  // Returns true, if iTunesLinksHandler can handle the given |url|.
  static bool CanHandleUrl(const GURL& url) {
    if (!IsITunesProductUrl(url))
      return false;
    // Valid iTunes URL structure:
    // DOMAIN/OPTIONAL_REGION_CODE/MEDIA_TYPE/MEDIA_NAME/ID?PARAMETERS
    // Check the URL media type, to determine if it is supported.
    base::FilePath path;
    if (!net::FileURLToFilePath(url, &path))
      return false;
    std::vector<base::FilePath::StringType> path_components;
    path.GetComponents(&path_components);
    // GetComponents considers "/" as the first component.
    if (path_components.size() < kITunesUrlPathMinComponentsCount)
      return false;
    size_t media_type_index = kITunesUrlMediaTypeComponentDefaultIndex;
    DCHECK(media_type_index > 0);
    // If there is no reigon code in the URL then media type has to appear
    // earlier in the URL.
    if (path_components[kITunesUrlRegionComponentDefaultIndex].size() != 2)
      media_type_index--;
    return path_components[media_type_index] == kITunesAppPathIdentifier;
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
  // Don't handle iTunse URL in off the record mode.
  if (web_state->GetBrowserState()->IsOffTheRecord())
    return;

  GURL url = navigation_context->GetUrl();
  // Whenever a navigation to iTunes product url is finished, launch StoreKit.
  if (ITunesLinksHandlerWebStatePolicyDecider::CanHandleUrl(url)) {
    ITunesUrlsStoreKitHandlingResult handling_result =
        ITunesUrlsStoreKitHandlingResult::kSingleAppUrlHandled;
    // If the url is iTunes product url, then this navigation should not be
    // committed, as the policy decider's ShouldAllowResponse will return false.
    DCHECK(!navigation_context->HasCommitted());
    StoreKitTabHelper* tab_helper = StoreKitTabHelper::FromWebState(web_state);
    if (tab_helper) {
      base::RecordAction(
          base::UserMetricsAction("ITunesLinksHandler_StoreKitLaunched"));
      tab_helper->OpenAppStore(ExtractITunesProductParameters(url));
    } else {
      handling_result = ITunesUrlsStoreKitHandlingResult::kUrlHandlingFailed;
    }
    RecordStoreKitHandlingResult(handling_result);
  }
}

void ITunesLinksHandlerTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state->RemoveObserver(this);
}
