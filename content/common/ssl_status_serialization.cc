// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/ssl_status_serialization.h"

#include <stdint.h>

#include "base/logging.h"
#include "base/pickle.h"

namespace {

// Checks that an integer |security_style| is a valid SecurityStyle enum
// value. Returns true if valid, false otherwise.
bool CheckSecurityStyle(int security_style) {
  switch (security_style) {
    case content::SECURITY_STYLE_UNKNOWN:
    case content::SECURITY_STYLE_UNAUTHENTICATED:
    case content::SECURITY_STYLE_AUTHENTICATION_BROKEN:
    case content::SECURITY_STYLE_WARNING:
    case content::SECURITY_STYLE_AUTHENTICATED:
      return true;
  }
  return false;
}

}  // namespace

namespace content {

std::string SerializeSecurityInfo(const SSLStatus& ssl_status) {
  base::Pickle pickle;
  pickle.WriteInt(ssl_status.security_style);
  pickle.WriteInt(ssl_status.cert_id);
  pickle.WriteUInt32(ssl_status.cert_status);
  pickle.WriteInt(ssl_status.security_bits);
  pickle.WriteInt(ssl_status.key_exchange_info);
  pickle.WriteInt(ssl_status.connection_status);
  pickle.WriteInt(ssl_status.signed_certificate_timestamp_ids.size());
  for (SignedCertificateTimestampIDStatusList::const_iterator iter =
           ssl_status.signed_certificate_timestamp_ids.begin();
       iter != ssl_status.signed_certificate_timestamp_ids.end(); ++iter) {
    pickle.WriteInt(iter->id);
    pickle.WriteUInt16(iter->status);
  }
  return std::string(static_cast<const char*>(pickle.data()), pickle.size());
}

bool DeserializeSecurityInfo(const std::string& state, SSLStatus* ssl_status) {
  *ssl_status = SSLStatus();

  if (state.empty()) {
    // No SSL used.
    return true;
  }

  base::Pickle pickle(state.data(), static_cast<int>(state.size()));
  base::PickleIterator iter(pickle);
  int security_style;
  int num_scts_to_read;
  if (!iter.ReadInt(&security_style) || !iter.ReadInt(&ssl_status->cert_id) ||
      !iter.ReadUInt32(&ssl_status->cert_status) ||
      !iter.ReadInt(&ssl_status->security_bits) ||
      !iter.ReadInt(&ssl_status->key_exchange_info) ||
      !iter.ReadInt(&ssl_status->connection_status) ||
      !iter.ReadInt(&num_scts_to_read)) {
    *ssl_status = SSLStatus();
    return false;
  }

  if (!CheckSecurityStyle(security_style)) {
    *ssl_status = SSLStatus();
    return false;
  }

  ssl_status->security_style = static_cast<SecurityStyle>(security_style);

  // Sanity check |security_bits|: the only allowed negative value is -1.
  if (ssl_status->security_bits < -1) {
    *ssl_status = SSLStatus();
    return false;
  }

  // Sanity check |key_exchange_info|: 0 or greater.
  if (ssl_status->key_exchange_info < 0) {
    *ssl_status = SSLStatus();
    return false;
  }

  for (; num_scts_to_read > 0; --num_scts_to_read) {
    int id;
    uint16_t status;
    if (!iter.ReadInt(&id) || !iter.ReadUInt16(&status)) {
      *ssl_status = SSLStatus();
      return false;
    }

    ssl_status->signed_certificate_timestamp_ids.push_back(
        SignedCertificateTimestampIDAndStatus(
            id, static_cast<net::ct::SCTVerifyStatus>(status)));
  }

  return true;
}

}  // namespace content
