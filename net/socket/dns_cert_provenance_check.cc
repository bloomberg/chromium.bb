// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/dns_cert_provenance_check.h"

#include <nspr.h>
#include <hasht.h>
#include <sechash.h>

#include <string>

#include "base/non_thread_safe.h"
#include "net/base/completion_callback.h"
#include "net/base/dns_util.h"
#include "net/base/dnsrr_resolver.h"
#include "net/base/net_log.h"
#include "net/base/net_errors.h"

namespace net {

namespace {

class DNSCertProvenanceChecker : public NonThreadSafe {
 public:
  DNSCertProvenanceChecker(const std::string hostname,
                           DnsRRResolver* dnsrr_resolver,
                           const std::vector<base::StringPiece>& der_certs)
      : hostname_(hostname),
        dnsrr_resolver_(dnsrr_resolver),
        der_certs_(der_certs.size()),
        handle_(DnsRRResolver::kInvalidHandle),
        ALLOW_THIS_IN_INITIALIZER_LIST(callback_(
              this, &DNSCertProvenanceChecker::ResolutionComplete)) {
    for (size_t i = 0; i < der_certs.size(); i++)
      der_certs_[i] = der_certs[i].as_string();
  }

  void Start() {
    DCHECK(CalledOnValidThread());

    if (der_certs_.empty())
      return;

    uint8 fingerprint[SHA1_LENGTH];
    SECStatus rv = HASH_HashBuf(
        HASH_AlgSHA1, fingerprint, (uint8*) der_certs_[0].data(),
        der_certs_[0].size());
    DCHECK_EQ(SECSuccess, rv);
    char fingerprint_hex[SHA1_LENGTH * 2 + 1];
    for (unsigned i = 0; i < sizeof(fingerprint); i++) {
      static const char hextable[] = "0123456789abcdef";
      fingerprint_hex[i*2] = hextable[fingerprint[i] >> 4];
      fingerprint_hex[i*2 + 1] = hextable[fingerprint[i] & 15];
    }
    fingerprint_hex[SHA1_LENGTH * 2] = 0;

    static const char kBaseCertName[] = ".certs.links.org";
    domain_.assign(fingerprint_hex);
    domain_.append(kBaseCertName);

    handle_ = dnsrr_resolver_->Resolve(
        domain_, kDNS_TXT, 0 /* flags */, &callback_, &response_,
        0 /* priority */, BoundNetLog());
    if (handle_ == DnsRRResolver::kInvalidHandle) {
      LOG(ERROR) << "Failed to resolve " << domain_ << " for " << hostname_;
      delete this;
    }
  }

 private:
  void ResolutionComplete(int status) {
    DCHECK(CalledOnValidThread());

    if (status == ERR_NAME_NOT_RESOLVED ||
        (status == OK && response_.rrdatas.empty())) {
      LOG(ERROR) << "FAILED"
                 << " hostname:" << hostname_
                 << " domain:" << domain_;
    } else if (status == OK) {
      LOG(ERROR) << "GOOD"
                 << " hostname:" << hostname_
                 << " resp:" << response_.rrdatas[0];
    } else {
      LOG(ERROR) << "Unknown error " << status << " for " << domain_;
    }

    delete this;
  }

  const std::string hostname_;
  std::string domain_;
  DnsRRResolver* const dnsrr_resolver_;
  std::vector<std::string> der_certs_;
  RRResponse response_;
  DnsRRResolver::Handle handle_;
  CompletionCallbackImpl<DNSCertProvenanceChecker> callback_;
};

}  // anonymous namespace

void DoAsyncDNSCertProvenanceVerification(
    const std::string& hostname,
    DnsRRResolver* dnsrr_resolver,
    const std::vector<base::StringPiece>& der_certs) {
  DNSCertProvenanceChecker* c(new DNSCertProvenanceChecker(
      hostname, dnsrr_resolver, der_certs));
  c->Start();
}

}  // namespace net
