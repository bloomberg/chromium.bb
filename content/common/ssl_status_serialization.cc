// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/ssl_status_serialization.h"

#include "base/logging.h"
#include "base/pickle.h"

namespace content {

std::string SerializeSecurityInfo(int cert_id,
                                  net::CertStatus cert_status,
                                  int security_bits,
                                  int ssl_connection_status) {
  Pickle pickle;
  pickle.WriteInt(cert_id);
  pickle.WriteUInt32(cert_status);
  pickle.WriteInt(security_bits);
  pickle.WriteInt(ssl_connection_status);
  return std::string(static_cast<const char*>(pickle.data()), pickle.size());
}

bool DeserializeSecurityInfo(const std::string& state,
                             int* cert_id,
                             net::CertStatus* cert_status,
                             int* security_bits,
                             int* ssl_connection_status) {
  DCHECK(cert_id && cert_status && security_bits && ssl_connection_status);
  if (state.empty()) {
    // No SSL used.
    *cert_id = 0;
    // The following are not applicable and are set to the default values.
    *cert_status = 0;
    *security_bits = -1;
    *ssl_connection_status = 0;
    return false;
  }

  Pickle pickle(state.data(), static_cast<int>(state.size()));
  PickleIterator iter(pickle);
  return pickle.ReadInt(&iter, cert_id) &&
         pickle.ReadUInt32(&iter, cert_status) &&
         pickle.ReadInt(&iter, security_bits) &&
         pickle.ReadInt(&iter, ssl_connection_status);
}

}  // namespace content
