// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_CLIENT_AUTH_REQUESTOR_MOCK_H_
#define CHROME_BROWSER_SSL_SSL_CLIENT_AUTH_REQUESTOR_MOCK_H_

#include "base/memory/ref_counted.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace net {
class HttpNetworkSession;
class SSLCertRequestInfo;
class URLRequest;
class X509Certificate;
}

class SSLClientAuthRequestorMock
    : public base::RefCountedThreadSafe<SSLClientAuthRequestorMock> {
 public:
  SSLClientAuthRequestorMock(
      net::URLRequest* request,
      net::SSLCertRequestInfo* cert_request_info);

  MOCK_METHOD1(CertificateSelected, void(net::X509Certificate* cert));

  net::SSLCertRequestInfo* cert_request_info_;
  net::HttpNetworkSession* http_network_session_;

 protected:
  friend class base::RefCountedThreadSafe<SSLClientAuthRequestorMock>;
  virtual ~SSLClientAuthRequestorMock();
};

#endif  // CHROME_BROWSER_SSL_SSL_CLIENT_AUTH_REQUESTOR_MOCK_H_
