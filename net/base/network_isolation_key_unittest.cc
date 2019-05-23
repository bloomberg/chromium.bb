// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_isolation_key.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {

TEST(NetworkIsolationKeyTest, EmptyKey) {
  NetworkIsolationKey key;
  EXPECT_FALSE(key.IsFullyPopulated());
  EXPECT_EQ(std::string(), key.ToString());
  EXPECT_TRUE(key.IsTransient());
}

TEST(NetworkIsolationKeyTest, NonEmptyKey) {
  url::Origin origin = url::Origin::Create(GURL("http://a.test/"));
  NetworkIsolationKey key(origin);
  EXPECT_TRUE(key.IsFullyPopulated());
  EXPECT_EQ(origin.Serialize(), key.ToString());
  EXPECT_FALSE(key.IsTransient());
}

TEST(NetworkIsolationKeyTest, OpaqueOriginKey) {
  url::Origin origin_data =
      url::Origin::Create(GURL("data:text/html,<body>Hello World</body>"));
  NetworkIsolationKey key(origin_data);
  EXPECT_TRUE(key.IsFullyPopulated());
  EXPECT_EQ(std::string(), key.ToString());
  EXPECT_TRUE(key.IsTransient());
}

}  // namespace net
