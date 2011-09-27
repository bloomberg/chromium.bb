// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ssl/ssl_cert_error_handler.h"

#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/browser/ssl/ssl_policy.h"
#include "net/base/cert_status_flags.h"
#include "net/base/x509_certificate.h"

SSLCertErrorHandler::SSLCertErrorHandler(
    ResourceDispatcherHost* rdh,
    net::URLRequest* request,
    ResourceType::Type resource_type,
    const net::SSLInfo& ssl_info,
    bool is_hsts_host)
    : SSLErrorHandler(rdh, request, resource_type),
      ssl_info_(ssl_info),
      cert_error_(net::MapCertStatusToNetError(ssl_info.cert_status)),
      is_hsts_host_(is_hsts_host) {
  DCHECK(request == resource_dispatcher_host_->GetURLRequest(request_id_));
}

SSLCertErrorHandler* SSLCertErrorHandler::AsSSLCertErrorHandler() {
  return this;
}

void SSLCertErrorHandler::OnDispatchFailed() {
  CancelRequest();
}

void SSLCertErrorHandler::OnDispatched() {
  manager_->policy()->OnCertError(this);
}

SSLCertErrorHandler::~SSLCertErrorHandler() {}
