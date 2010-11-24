// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_CHROME_DNS_CERT_PROVENANCE_CHECKER_FACTORY
#define CHROME_BROWSER_NET_CHROME_DNS_CERT_PROVENANCE_CHECKER_FACTORY
#pragma once

#include "net/socket/dns_cert_provenance_checker.h"

// WARNING: This factory abstraction is needed because we cannot link NSS code
// into a .cc file which is included by both Chrome and Chrome Frame. This
// factory exists so that common code links only against the factory code.
// Chrome specific code will link against the NSS using code in
// chrome_dns_cert_provenance_checker.cc and hand a function pointer to this
// code.

namespace net {
class DnsRRResolver;
}

class ChromeURLRequestContext;

// A DnsCertProvenanceCheckerFactory is a function pointer to a factory
// function for DnsCertProvenanceCheckerFactory objects.
typedef net::DnsCertProvenanceChecker* (*DnsCertProvenanceCheckerFactory) (
    net::DnsRRResolver* dnsrr_resolver,
    ChromeURLRequestContext* url_req_context);

// Return a new DnsCertProvenanceChecker. Caller takes ownership. May return
// NULL if no factory function has been set.
net::DnsCertProvenanceChecker* CreateDnsCertProvenanceChecker(
    net::DnsRRResolver* dnsrr_resolver,
    ChromeURLRequestContext* url_req_context);

void SetDnsCertProvenanceCheckerFactory(DnsCertProvenanceCheckerFactory);

#endif  // CHROME_BROWSER_NET_CHROME_DNS_CERT_PROVENANCE_CHECKER_FACTORY
