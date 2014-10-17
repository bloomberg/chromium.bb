// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_SSL_STATUS_H_
#define IOS_WEB_PUBLIC_SSL_STATUS_H_

#include "ios/web/public/security_style.h"
#include "net/cert/cert_status_flags.h"

namespace web {

// Collects the SSL information for this NavigationItem.
struct SSLStatus {
  // Flags used for the page security content status.
  enum ContentStatusFlags {
    // HTTP page, or HTTPS page with no insecure content.
    NORMAL_CONTENT             = 0,

    // HTTPS page containing "displayed" HTTP resources (e.g. images, CSS).
    DISPLAYED_INSECURE_CONTENT = 1 << 0,

    // The RAN_INSECURE_CONTENT flag is intentionally omitted on iOS because
    // there is no way to tell when insecure content is run in a web view.
  };

  SSLStatus();
  ~SSLStatus();

  bool Equals(const SSLStatus& status) const {
    return security_style == status.security_style &&
           cert_id == status.cert_id &&
           cert_status == status.cert_status &&
           security_bits == status.security_bits &&
           content_status == status.content_status;
  }

  web::SecurityStyle security_style;
  int cert_id;
  net::CertStatus cert_status;
  int security_bits;
  int connection_status;
  // A combination of the ContentStatusFlags above.
  int content_status;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_SSL_STATUS_H_
