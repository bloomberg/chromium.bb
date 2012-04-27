// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_client_auth_requestor_mock.h"

#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"

SSLClientAuthRequestorMock::SSLClientAuthRequestorMock(
    net::URLRequest* request,
    net::SSLCertRequestInfo* cert_request_info)
    : cert_request_info_(cert_request_info),
      http_network_session_(
          request->context()->http_transaction_factory()->GetSession()) {
}

SSLClientAuthRequestorMock::~SSLClientAuthRequestorMock() {}
