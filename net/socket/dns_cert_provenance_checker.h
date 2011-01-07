// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_DNS_CERT_PROVENANCE_CHECKER_H
#define NET_SOCKET_DNS_CERT_PROVENANCE_CHECKER_H

#include <string>
#include <vector>

#include "base/string_piece.h"

namespace net {

class DnsRRResolver;

// DnsCertProvenanceChecker is an interface for asynchronously checking HTTPS
// certificates via a DNS side-channel.
class DnsCertProvenanceChecker {
 public:
  class Delegate {
   public:
    virtual ~Delegate();

    virtual void OnDnsCertLookupFailed(
        const std::string& hostname,
        const std::vector<std::string>& der_certs) = 0;
  };

  virtual ~DnsCertProvenanceChecker();

  virtual void Shutdown() = 0;

  // DoAsyncVerification starts an asynchronous check for the given certificate
  // chain. It must be run on the network thread.
  virtual void DoAsyncVerification(
      const std::string& hostname,
      const std::vector<base::StringPiece>& der_certs) = 0;


 protected:
  // DoAsyncLookup performs a DNS lookup for the given name and certificate
  // chain. In the event that the lookup reports a failure, the Delegate is
  // called back.
  static void DoAsyncLookup(
      const std::string& hostname,
      const std::vector<base::StringPiece>& der_certs,
      DnsRRResolver* dnsrr_resolver,
      Delegate* delegate);

  // BuildEncryptedRecord encrypts the certificate chain to a fixed public key
  // and returns the encrypted blob. Since this code is reporting a possible
  // HTTPS failure, it would seem silly to use HTTPS to protect the uploaded
  // report.
  static std::string BuildEncryptedReport(
      const std::string& hostname,
      const std::vector<std::string>& der_certs);
};

}  // namespace net

#endif  // NET_SOCKET_DNS_CERT_PROVENANCE_CHECK_H
