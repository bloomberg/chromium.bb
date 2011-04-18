// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_client_auth_notification_details.h"

#include "net/base/ssl_cert_request_info.h"

SSLClientAuthNotificationDetails::SSLClientAuthNotificationDetails(
    const net::SSLCertRequestInfo* cert_request_info,
    net::X509Certificate* selected_cert)
    : cert_request_info_(cert_request_info),
      selected_cert_(selected_cert) {
}

bool SSLClientAuthNotificationDetails::IsSameHost(
    const net::SSLCertRequestInfo* cert_request_info) const {
  // TODO(mattm): should we also compare the DistinguishedNames, or is just
  // matching host&port sufficient?
  return cert_request_info_->host_and_port == cert_request_info->host_and_port;
}
