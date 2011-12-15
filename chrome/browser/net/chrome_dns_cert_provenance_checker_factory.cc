// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_dns_cert_provenance_checker_factory.h"

static DnsCertProvenanceCheckerFactory g_factory;

net::DnsCertProvenanceChecker* CreateDnsCertProvenanceChecker(
    net::DnsRRResolver* dnsrr_resolver,
    ChromeURLRequestContext* url_req_context) {
  if (!g_factory)
    return NULL;

  return g_factory(dnsrr_resolver, url_req_context);
}

void SetDnsCertProvenanceCheckerFactory(DnsCertProvenanceCheckerFactory f) {
  g_factory = f;
}
