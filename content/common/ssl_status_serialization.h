// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SSL_STATUS_SERIALIZATION_H_
#define CONTENT_COMMON_SSL_STATUS_SERIALIZATION_H_

#include <string>

#include "base/compiler_specific.h"
#include "content/common/content_export.h"
#include "content/public/common/ssl_status.h"

namespace content {

// Serializes the given security info.
CONTENT_EXPORT std::string SerializeSecurityInfo(
    int cert_id,
    net::CertStatus cert_status,
    int security_bits,
    int connection_status,
    const SignedCertificateTimestampIDStatusList&
        signed_certificate_timestamp_ids);

// Deserializes the given security info into |ssl_status|. Note that
// this returns the SecurityStyle and ContentStatus fields with default
// values. Returns true on success and false if the state couldn't be
// deserialized. If false, all fields in |ssl_status| will be set to their
// default values.
bool CONTENT_EXPORT
DeserializeSecurityInfo(const std::string& state,
                        SSLStatus* ssl_status) WARN_UNUSED_RESULT;

}  // namespace content

#endif  // CONTENT_COMMON_SSL_STATUS_SERIALIZATION_H_
