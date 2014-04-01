// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_session_key.h"

#include "testing/gtest/include/gtest/gtest.h"

using std::string;

namespace net {

namespace {

TEST(QuicSessionKeyTest, ToString) {
  HostPortPair google_host_port_pair("google.com", 10);

  QuicSessionKey google_http_key(google_host_port_pair, false,
                                 PRIVACY_MODE_DISABLED);
  string google_http_key_str = google_http_key.ToString();
  EXPECT_EQ("http://google.com:10", google_http_key_str);

  QuicSessionKey google_https_key(google_host_port_pair, true,
                                 PRIVACY_MODE_DISABLED);
  string google_https_key_str = google_https_key.ToString();
  EXPECT_EQ("https://google.com:10", google_https_key_str);

  QuicSessionKey private_http_key(google_host_port_pair, false,
                                  PRIVACY_MODE_ENABLED);
  string private_http_key_str = private_http_key.ToString();
  EXPECT_EQ("http://google.com:10/private", private_http_key_str);

  QuicSessionKey private_https_key(google_host_port_pair, true,
                                   PRIVACY_MODE_ENABLED);
  string private_https_key_str = private_https_key.ToString();
  EXPECT_EQ("https://google.com:10/private", private_https_key_str);
}

}  // namespace

}  // namespace net
