// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SSL_SSL_CLIENT_AUTH_HANDLER_MOCK_H_
#define CONTENT_BROWSER_SSL_SSL_CLIENT_AUTH_HANDLER_MOCK_H_
#pragma once

#include "content/browser/ssl/ssl_client_auth_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

class SSLClientAuthHandlerMock : public SSLClientAuthHandler {
 public:
  SSLClientAuthHandlerMock(
      net::URLRequest* request,
      net::SSLCertRequestInfo* cert_request_info);
  ~SSLClientAuthHandlerMock();

  MOCK_METHOD1(CertificateSelectedNoNotify, void(net::X509Certificate* cert));

 private:
  DISALLOW_COPY_AND_ASSIGN(SSLClientAuthHandlerMock);
};


#endif  // CONTENT_BROWSER_SSL_SSL_CLIENT_AUTH_HANDLER_MOCK_H_
