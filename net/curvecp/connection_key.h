// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CURVECP_CONNECTION_KEY_H_
#define NET_CURVECP_CONNECTION_KEY_H_
#pragma once

#include <string>

namespace net {

// A ConnectionKey uniquely identifies a connection.
// It represents the client's short-term public key and is 32 bytes.
class ConnectionKey {
 public:
  ConnectionKey();
  ConnectionKey(unsigned char bytes[]);
  ConnectionKey(const ConnectionKey& key);

  ConnectionKey& operator=(const ConnectionKey& other);
  bool operator==(const ConnectionKey& other) const;
  bool operator<(const ConnectionKey& other) const;

  std::string ToString() const;

 private:
  unsigned char key_[32];
};

}  // namespace net

#endif  // NET_CURVECP_CONNECTION_KEY_H_
