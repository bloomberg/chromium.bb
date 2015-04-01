// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NET_CLIENTS_CRW_JS_INJECTION_NETWORK_CLIENT_H_
#define IOS_WEB_NET_CLIENTS_CRW_JS_INJECTION_NETWORK_CLIENT_H_

#include "ios/net/clients/crn_forwarding_network_client.h"

namespace web {
// Used for UMA histogram and must be kept in sync with the histograms.xml file.
enum class InjectionResult : int {
  SUCCESS_INJECTED,
  FAIL_FIND_INJECTION_LOCATION,
  FAIL_INSUFFICIENT_CONTENT_LENGTH,
  FAIL_SELF_REFERRER,

  INJECTION_RESULT_COUNT
  // INJECTION_RESULT_COUNT must always be the last element in this enum
};
}  // namespace web

// Network client that injects a script tag into HTML and XHTML documents.
@interface CRWJSInjectionNetworkClient : CRNForwardingNetworkClient

// Returns YES if |response| has a "Content-Type" header approriate for
// injection. At this time this means if the "Content-Type" header is HTML
// or XHTML.
+ (BOOL)canHandleResponse:(NSURLResponse*)response;

@end

#endif  // IOS_WEB_NET_CLIENTS_CRW_JS_INJECTION_NETWORK_CLIENT_H_
