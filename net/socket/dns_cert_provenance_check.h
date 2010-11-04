// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_DNS_CERT_PROVENANCE_CHECK_H
#define NET_SOCKET_DNS_CERT_PROVENANCE_CHECK_H

#include <string>
#include <vector>

#include "base/string_piece.h"

namespace net {

class DnsRRResolver;

// DoAsyncDNSCertProvenanceVerification starts an asynchronous check for the
// given certificate chain. It must be run on the network thread.
void DoAsyncDNSCertProvenanceVerification(
    const std::string& hostname,
    DnsRRResolver* dnsrr_resolver,
    const std::vector<base::StringPiece>& der_certs);

}  // namespace net

#endif  // NET_SOCKET_DNS_CERT_PROVENANCE_CHECK_H
