// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/net/clients/crw_js_injection_network_client_factory.h"

#include "base/metrics/histogram.h"
#include "ios/web/net/clients/crw_js_injection_network_client.h"
#include "net/url_request/url_request.h"

@implementation CRWJSInjectionNetworkClientFactory

- (CRWJSInjectionNetworkClient*)
    clientHandlingResponse:(NSURLResponse*)response
                   request:(const net::URLRequest&)request {
  // Only add the JS Injection client to requests if response can be handled.
  // Only injects if the the request URL is the main frame's URL to weed out
  // subframes and resource requests.
  if ([CRWJSInjectionNetworkClient canHandleResponse:response] &&
      (request.url() == request.first_party_for_cookies())) {
    // Heuristic to avoid injection on XHR to the origin url. This will cause
    // some false negatives, so record UMA to monitor how many pages this.
    // JS injection for these false negatives will be handled by polling-based
    // injection.
    if (GURL(request.referrer()) == request.url()) {
      UMA_HISTOGRAM_ENUMERATION(
          "NetworkLayerJSInjection.Result",
          static_cast<int>(web::InjectionResult::FAIL_SELF_REFERRER),
          static_cast<int>(web::InjectionResult::INJECTION_RESULT_COUNT));
      return nil;
    }
    return [[[CRWJSInjectionNetworkClient alloc] init] autorelease];
  }
  return nil;
}

- (Class)clientClass {
  return [CRWJSInjectionNetworkClient class];
}

@end
