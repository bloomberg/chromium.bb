// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_ORIGIN_BOUND_CERT_SERVICE_H_
#define NET_BASE_ORIGIN_BOUND_CERT_SERVICE_H_
#pragma once

#include <string>

class GURL;

namespace net {

class OriginBoundCertStore;

// A class for creating and fetching origin bound certs.
class OriginBoundCertService {
 public:
  explicit OriginBoundCertService(OriginBoundCertStore* origin_bound_cert_store)
      : origin_bound_cert_store_(origin_bound_cert_store) {}

  // TODO(rkn): Specify certificate type (RSA or DSA).
  // TODO(rkn): Key generation can be time consuming, so this should have an
  // asynchronous interface.
  // Fetches the origin bound cert for the specified origin if one exists
  // and creates one otherwise. On success, |private_key_result| stores a
  // PrivateKeyInfo struct, and |cert_result| stores a DER-encoded certificate.
  bool GetOriginBoundCert(const GURL& url,
                          std::string* private_key_result,
                          std::string* cert_result);

 private:
  OriginBoundCertStore* origin_bound_cert_store_;
};

}  // namespace net

#endif  // NET_BASE_ORIGIN_BOUND_CERT_SERVICE_H_
