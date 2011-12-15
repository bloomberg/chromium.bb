// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_CHROME_DNS_CERT_PROVENANCE_CHECKER
#define CHROME_BROWSER_NET_CHROME_DNS_CERT_PROVENANCE_CHECKER
#pragma once

#include "net/socket/dns_cert_provenance_checker.h"

#include <string>
#include <vector>

#include "base/string_piece.h"

namespace net {
class DnsRRResolver;
}

class ChromeURLRequestContext;

// Factory function which creates ChromeDnsCertProvenanceChecker objects.
//
// WARNING: do not use this with anything other than the main
// ChromeURLRequestContext. Eventually we'll want to have the other contexts
// point to the main ChromeURLRequestContext, which then causes lifetime
// ordering issues wrt ChromeURLRequestContexts, since we're using a raw
// pointer, and we'll get shutdown ordering problems.
net::DnsCertProvenanceChecker* CreateChromeDnsCertProvenanceChecker(
    net::DnsRRResolver* dnsrr_resolver,
    ChromeURLRequestContext* url_req_context);

#endif  // CHROME_BROWSER_NET_CHROME_DNS_CERT_PROVENANCE_CHECKER
