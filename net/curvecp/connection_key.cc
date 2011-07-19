// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/curvecp/connection_key.h"

#include <string.h>

namespace net {

ConnectionKey::ConnectionKey() {
  memset(key_, 0, sizeof(key_));
}

ConnectionKey::ConnectionKey(unsigned char bytes[]) {
  memcpy(key_, bytes, sizeof(key_));
}

ConnectionKey::ConnectionKey(const ConnectionKey& other) {
  memcpy(key_, other.key_, sizeof(key_));
}

ConnectionKey& ConnectionKey::operator=(const ConnectionKey& other) {
  memcpy(key_, other.key_, sizeof(key_));
  return *this;
}

bool ConnectionKey::operator==(const ConnectionKey& other) const {
  return memcmp(key_, other.key_, sizeof(key_)) == 0;
}

bool ConnectionKey::operator<(const ConnectionKey& other) const {
  return memcmp(key_, other.key_, sizeof(key_)) < 0;
}

std::string ConnectionKey::ToString() const {
  // TODO(mbelshe):  make this a nice hex formatter
  return std::string("key") +
         std::string(reinterpret_cast<const char*>(key_), 32);
}

}  // namespace net
