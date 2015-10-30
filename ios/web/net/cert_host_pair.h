// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NET_CERT_HOST_PAIR_H_
#define IOS_WEB_NET_CERT_HOST_PAIR_H_

#include <string>

#include "base/memory/ref_counted.h"

namespace net {
class X509Certificate;
}

namespace web {

// Holds certificate-host pair. Implements operator less, hence can act as a key
// for a container.
struct CertHostPair {
  CertHostPair(const scoped_refptr<net::X509Certificate>& cert,
               const std::string& host);
  ~CertHostPair();

  bool operator<(const CertHostPair& other) const;

  scoped_refptr<net::X509Certificate> cert;
  std::string host;
};

}  // namespace web

#endif  // IOS_WEB_NET_CERT_HOST_PAIR_H_
