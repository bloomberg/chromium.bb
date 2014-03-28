// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_session_key.h"

using std::string;

namespace net {

QuicSessionKey::QuicSessionKey() {}

QuicSessionKey::QuicSessionKey(const HostPortPair& host_port_pair,
                               bool is_https,
                               PrivacyMode privacy_mode)
    : host_port_pair_(host_port_pair),
      is_https_(is_https),
      privacy_mode_(privacy_mode) {}

QuicSessionKey::QuicSessionKey(const string& host,
                               uint16 port,
                               bool is_https,
                               PrivacyMode privacy_mode)
    : host_port_pair_(host, port),
      is_https_(is_https),
      privacy_mode_(privacy_mode) {}

QuicSessionKey::~QuicSessionKey() {}

bool QuicSessionKey::operator<(const QuicSessionKey& other) const {
  if (!host_port_pair_.Equals(other.host_port_pair_)) {
    return host_port_pair_ < other.host_port_pair_;
  }
  if (is_https_ != other.is_https_) {
    return is_https_ < other.is_https_;
  }
  return privacy_mode_ < other.privacy_mode_;
}

bool QuicSessionKey::operator==(const QuicSessionKey& other) const {
  return is_https_ == other.is_https_ &&
      privacy_mode_ == other.privacy_mode_ &&
      host_port_pair_.Equals(other.host_port_pair_);
}

string QuicSessionKey::ToString() const {
  return (is_https_ ? "https://" : "http://") + host_port_pair_.ToString() +
      (privacy_mode_ == PRIVACY_MODE_ENABLED ? "/private" : "");
}

}  // namespace net
