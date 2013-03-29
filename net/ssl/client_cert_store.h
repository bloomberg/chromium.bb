// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_CLIENT_CERT_STORE_H_
#define NET_SSL_CLIENT_CERT_STORE_H_

#include "base/basictypes.h"
#include "net/base/net_export.h"
#include "net/cert/x509_certificate.h"

namespace net {

class SSLCertRequestInfo;

class NET_EXPORT ClientCertStore {
 public:
  virtual ~ClientCertStore() {}

  virtual bool GetClientCerts(const SSLCertRequestInfo& cert_request_info,
                              CertificateList* selected_certs) = 0;
 protected:
  ClientCertStore() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ClientCertStore);
};

}  // namespace net

#endif  // NET_SSL_CLIENT_CERT_STORE_H_
