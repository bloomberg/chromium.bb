// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_dns_cert_provenance_checker.h"

namespace {

class ChromeDnsCertProvenanceChecker :
    public net::DnsCertProvenanceChecker,
    public net::DnsCertProvenanceChecker::Delegate {
 public:
  ChromeDnsCertProvenanceChecker(
      net::DnsRRResolver* dnsrr_resolver,
      ChromeURLRequestContext* url_req_context)
      : dnsrr_resolver_(dnsrr_resolver),
        url_req_context_(url_req_context) {
  }

  // DnsCertProvenanceChecker interface
  virtual void DoAsyncVerification(
      const std::string& hostname,
      const std::vector<base::StringPiece>& der_certs) {
    net::DnsCertProvenanceChecker::DoAsyncLookup(hostname, der_certs,
                                                 dnsrr_resolver_, this);
  }

  // DnsCertProvenanceChecker::Delegate interface
  virtual void OnDnsCertLookupFailed(
      const std::string& hostname,
      const std::vector<std::string>& der_certs) {
    // Currently unimplemented.
  }

 private:
  net::DnsRRResolver* const dnsrr_resolver_;
  ChromeURLRequestContext* const url_req_context_;
};

}  // namespace

net::DnsCertProvenanceChecker* CreateChromeDnsCertProvenanceChecker(
    net::DnsRRResolver* dnsrr_resolver,
    ChromeURLRequestContext* url_req_context) {
  return new ChromeDnsCertProvenanceChecker(dnsrr_resolver, url_req_context);
}
