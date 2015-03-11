// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_CLIENT_AUTH_REQUESTOR_MOCK_H_
#define CHROME_BROWSER_SSL_SSL_CLIENT_AUTH_REQUESTOR_MOCK_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {
class ClientCertificateDelegate;
}

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
      const scoped_refptr<net::SSLCertRequestInfo>& cert_request_info);

  scoped_ptr<content::ClientCertificateDelegate> CreateDelegate();

  MOCK_METHOD1(CertificateSelected, void(net::X509Certificate* cert));
  MOCK_METHOD0(CancelCertificateSelection, void());

  scoped_refptr<net::SSLCertRequestInfo> cert_request_info_;

 protected:
  friend class base::RefCountedThreadSafe<SSLClientAuthRequestorMock>;
  virtual ~SSLClientAuthRequestorMock();
};

#endif  // CHROME_BROWSER_SSL_SSL_CLIENT_AUTH_REQUESTOR_MOCK_H_
