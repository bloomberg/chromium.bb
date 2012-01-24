// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ssl/ssl_client_auth_handler_mock.h"

SSLClientAuthHandlerMock::SSLClientAuthHandlerMock(
    net::URLRequest* request,
    net::SSLCertRequestInfo* cert_request_info)
    : SSLClientAuthHandler(request, cert_request_info) {
}

SSLClientAuthHandlerMock::~SSLClientAuthHandlerMock() {
  // Hack to avoid destructor calling request_->ContinueWithCertificate.
  OnRequestCancelled();
}
