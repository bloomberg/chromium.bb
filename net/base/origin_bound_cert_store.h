// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_ORIGIN_BOUND_CERT_STORE_H_
#define NET_BASE_ORIGIN_BOUND_CERT_STORE_H_
#pragma once

#include <string>

class GURL;

namespace net {

// An interface for storing and retrieving origin bound certs. Origin bound
// certificates are specified in
// http://balfanz.github.com/tls-obc-spec/draft-balfanz-tls-obc-00.html.

class OriginBoundCertStore {
 public:
  virtual bool HasOriginBoundCert(const GURL& url) = 0;

  // TODO(rkn): Specify certificate type (RSA or DSA).
  // TODO(rkn): Key generation can be time consuming, so this should have an
  // asynchronous interface.
  // The output is stored in |private_key_result| and |cert_result|.
  virtual bool GetOriginBoundCert(const GURL& url,
                                  std::string* private_key_result,
                                  std::string* cert_result) = 0;

  virtual bool SetOriginBoundCert(const GURL& url,
                                  const std::string& private_key,
                                  const std::string& cert) = 0;
};

}  // namespace net

#endif  // NET_BASE_ORIGIN_BOUND_CERT_STORE_H_
