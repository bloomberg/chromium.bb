// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_SESSION_KEY_H_
#define NET_QUIC_QUIC_SESSION_KEY_H_

#include <string>

#include "net/base/host_port_pair.h"
#include "net/base/net_export.h"

namespace net {

// The key used to identify sessions. Includes the hostname, port, and scheme.
class NET_EXPORT_PRIVATE QuicSessionKey {
 public:
  QuicSessionKey();
  QuicSessionKey(const HostPortPair& host_port_pair, bool is_https);
  QuicSessionKey(const std::string& host, uint16 port, bool is_https);
  ~QuicSessionKey();

  // Needed to be an element of std::set.
  bool operator<(const QuicSessionKey& other) const;
  bool operator==(const QuicSessionKey& other) const;

  // ToString() will convert the QuicSessionKey to "scheme:hostname:port".
  // "scheme" would either be "http" or "https" based on |is_https|.
  std::string ToString() const;

  const HostPortPair& host_port_pair() const {
    return host_port_pair_;
  }

  const std::string& host() const { return host_port_pair_.host(); }

  uint16 port() const { return host_port_pair_.port(); }

  bool is_https() const { return is_https_; }

 private:
  HostPortPair host_port_pair_;
  bool is_https_;
};

}  // namespace net

#endif  // NET_QUIC_QUIC_SESSION_KEY_H_
