// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/ssl_status.h"

#include "net/ssl/ssl_info.h"

namespace content {

SSLStatus::SSLStatus()
    : security_style(SECURITY_STYLE_UNKNOWN),
      cert_id(0),
      cert_status(0),
      security_bits(-1),
      key_exchange_info(0),
      connection_status(0),
      content_status(NORMAL_CONTENT) {
}

SSLStatus::SSLStatus(SecurityStyle security_style,
                     int cert_id,
                     const SignedCertificateTimestampIDStatusList&
                         signed_certificate_timestamp_ids,
                     const net::SSLInfo& ssl_info)
    : security_style(security_style),
      cert_id(cert_id),
      cert_status(ssl_info.cert_status),
      security_bits(ssl_info.security_bits),
      key_exchange_info(ssl_info.key_exchange_info),
      connection_status(ssl_info.connection_status),
      content_status(NORMAL_CONTENT),
      signed_certificate_timestamp_ids(signed_certificate_timestamp_ids) {}

SSLStatus::~SSLStatus() {}

}  // namespace content
