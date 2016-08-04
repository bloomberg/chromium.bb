// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/ssl_status_serialization.h"

#include <stdint.h>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
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

// Checks that an integer |sct_status| is a valid net::ct::SCTVerifyStatus enum
// value. Returns true if valid, false otherwise.
bool CheckSCTStatus(uint32_t sct_status) {
  switch (sct_status) {
    case net::ct::SCT_STATUS_LOG_UNKNOWN:
    case net::ct::SCT_STATUS_INVALID:
    case net::ct::SCT_STATUS_OK:
    case net::ct::SCT_STATUS_MAX:
      return true;
    case net::ct::SCT_STATUS_NONE:
      // SCT_STATUS_NONE should never happen, so it isn't valid to
      // receive a status of NONE in a serialized SSLStatus.
      return false;
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
  pickle.WriteUInt32(ssl_status.sct_statuses.size());
  for (const auto& sct_status : ssl_status.sct_statuses) {
    pickle.WriteUInt32(sct_status);
  }
  pickle.WriteBool(ssl_status.pkp_bypassed);
  return std::string(static_cast<const char*>(pickle.data()), pickle.size());
}

bool DeserializeSecurityInfo(const std::string& state, SSLStatus* ssl_status) {
  *ssl_status = SSLStatus();

  if (state.empty()) {
    // No SSL used.
    return true;
  }

  base::Pickle pickle(state.data(), base::checked_cast<int>(state.size()));
  base::PickleIterator iter(pickle);
  int security_style;
  if (!iter.ReadInt(&security_style) || !iter.ReadInt(&ssl_status->cert_id) ||
      !iter.ReadUInt32(&ssl_status->cert_status) ||
      !iter.ReadInt(&ssl_status->security_bits) ||
      !iter.ReadInt(&ssl_status->key_exchange_info) ||
      !iter.ReadInt(&ssl_status->connection_status)) {
    *ssl_status = SSLStatus();
    return false;
  }

  uint32_t num_sct_statuses;
  if (!iter.ReadUInt32(&num_sct_statuses)) {
    return false;
  }

  for (uint32_t i = 0; i < num_sct_statuses; i++) {
    uint32_t sct_status;
    if (!iter.ReadUInt32(&sct_status) || !CheckSCTStatus(sct_status)) {
      *ssl_status = SSLStatus();
      return false;
    }
    ssl_status->sct_statuses.push_back(
        static_cast<net::ct::SCTVerifyStatus>(sct_status));
  }

  if (!iter.ReadBool(&ssl_status->pkp_bypassed)) {
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

  return true;
}

}  // namespace content
