// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_ORIGIN_BOUND_CERT_SERVICE_H_
#define NET_BASE_ORIGIN_BOUND_CERT_SERVICE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "net/base/origin_bound_cert_store.h"
#include "googleurl/src/gurl.h" // TODO(rkn): This feels wrong.

namespace net {

// A class for creating and fetching origin bound certs.
class OriginBoundCertService {
 public:

  OriginBoundCertService(OriginBoundCertStore* origin_bound_cert_store)
      : origin_bound_cert_store_(origin_bound_cert_store) {}

  // TODO(rkn): Specify certificate type (RSA or DSA).
  // TODO(rkn): Key generation can be time consuming, so this should have an
  // asynchronous interface.
  // This function will fetch the origin bound cert for the specified origin
  // if one exists and it will create one otherwise.
  bool GetOriginBoundCert(const GURL& url,
                          std::string* private_key_result,
                          std::string* cert_result);

  static std::string GetCertOriginFromURL(const GURL& url);

 private:
  OriginBoundCertStore* origin_bound_cert_store_;
};

}  // namespace net

#endif  // NET_BASE_ORIGIN_BOUND_CERT_SERVICE_H_
