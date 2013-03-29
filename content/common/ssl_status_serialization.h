// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SSL_STATUS_SERIALIZATION_H_
#define CONTENT_COMMON_SSL_STATUS_SERIALIZATION_H_

#include <string>

#include "net/cert/cert_status_flags.h"

namespace content {

// Convenience methods for serializing/deserializing the security info.
std::string SerializeSecurityInfo(int cert_id,
                                  net::CertStatus cert_status,
                                  int security_bits,
                                  int connection_status);

bool DeserializeSecurityInfo(const std::string& state,
                             int* cert_id,
                             net::CertStatus* cert_status,
                             int* security_bits,
                             int* connection_status);

}  // namespace content

#endif  // CONTENT_COMMON_SSL_STATUS_SERIALIZATION_H_
