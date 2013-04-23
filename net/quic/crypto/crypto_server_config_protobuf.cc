// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/crypto_server_config_protobuf.h"

#include "base/stl_util.h"

namespace net {

QuicServerConfigProtobuf::QuicServerConfigProtobuf() {
}

QuicServerConfigProtobuf::~QuicServerConfigProtobuf() {
  STLDeleteElements(&keys_);
}

}  // namespace net
