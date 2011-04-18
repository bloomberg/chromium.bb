// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_CLIENT_AUTH_NOTIFICATION_DETAILS_H_
#define CHROME_BROWSER_SSL_SSL_CLIENT_AUTH_NOTIFICATION_DETAILS_H_

#include "base/basictypes.h"

namespace net {
class X509Certificate;
class SSLCertRequestInfo;
}

class SSLClientAuthNotificationDetails {
 public:
  SSLClientAuthNotificationDetails(
      const net::SSLCertRequestInfo* cert_request_info,
      net::X509Certificate* selected_cert);

  bool IsSameHost(const net::SSLCertRequestInfo* cert_request_info) const;
  net::X509Certificate* selected_cert() const { return selected_cert_; }

 private:
  // Notifications are synchronous, so we don't need to hold our own references.
  const net::SSLCertRequestInfo* cert_request_info_;
  net::X509Certificate* selected_cert_;

  DISALLOW_COPY_AND_ASSIGN(SSLClientAuthNotificationDetails);
};

#endif  // CHROME_BROWSER_SSL_SSL_CLIENT_AUTH_NOTIFICATION_DETAILS_H_
