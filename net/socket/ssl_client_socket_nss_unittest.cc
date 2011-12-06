// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/ssl_client_socket_nss.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace net {

// Verifies that SSLClientSocketNSS::ClearSessionCache can be called without
// explicit NSS initialization.
TEST(SSLClientSocketNSSTest, ClearSessionCache) {
  SSLClientSocketNSS::ClearSessionCache();
}

}  // namespace net
