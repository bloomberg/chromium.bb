// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_util.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(NetUtilTest, IsLocalhost) {
  EXPECT_TRUE(IsLocalhost("localhost"));
  EXPECT_TRUE(IsLocalhost("localHosT"));
  EXPECT_TRUE(IsLocalhost("localhost."));
  EXPECT_TRUE(IsLocalhost("localHost."));
  EXPECT_TRUE(IsLocalhost("localhost.localdomain"));
  EXPECT_TRUE(IsLocalhost("localhost.localDOMain"));
  EXPECT_TRUE(IsLocalhost("localhost.localdomain."));
  EXPECT_TRUE(IsLocalhost("localhost6"));
  EXPECT_TRUE(IsLocalhost("localhost6."));
  EXPECT_TRUE(IsLocalhost("localhost6.localdomain6"));
  EXPECT_TRUE(IsLocalhost("localhost6.localdomain6."));
  EXPECT_TRUE(IsLocalhost("127.0.0.1"));
  EXPECT_TRUE(IsLocalhost("127.0.1.0"));
  EXPECT_TRUE(IsLocalhost("127.1.0.0"));
  EXPECT_TRUE(IsLocalhost("127.0.0.255"));
  EXPECT_TRUE(IsLocalhost("127.0.255.0"));
  EXPECT_TRUE(IsLocalhost("127.255.0.0"));
  EXPECT_TRUE(IsLocalhost("::1"));
  EXPECT_TRUE(IsLocalhost("0:0:0:0:0:0:0:1"));
  EXPECT_TRUE(IsLocalhost("foo.localhost"));
  EXPECT_TRUE(IsLocalhost("foo.localhost."));
  EXPECT_TRUE(IsLocalhost("foo.localhoST"));
  EXPECT_TRUE(IsLocalhost("foo.localhoST."));

  EXPECT_FALSE(IsLocalhost("localhostx"));
  EXPECT_FALSE(IsLocalhost("localhost.x"));
  EXPECT_FALSE(IsLocalhost("foo.localdomain"));
  EXPECT_FALSE(IsLocalhost("foo.localdomain.x"));
  EXPECT_FALSE(IsLocalhost("localhost6x"));
  EXPECT_FALSE(IsLocalhost("localhost.localdomain6"));
  EXPECT_FALSE(IsLocalhost("localhost6.localdomain"));
  EXPECT_FALSE(IsLocalhost("127.0.0.1.1"));
  EXPECT_FALSE(IsLocalhost(".127.0.0.255"));
  EXPECT_FALSE(IsLocalhost("::2"));
  EXPECT_FALSE(IsLocalhost("::1:1"));
  EXPECT_FALSE(IsLocalhost("0:0:0:0:1:0:0:1"));
  EXPECT_FALSE(IsLocalhost("::1:1"));
  EXPECT_FALSE(IsLocalhost("0:0:0:0:0:0:0:0:1"));
  EXPECT_FALSE(IsLocalhost("foo.localhost.com"));
  EXPECT_FALSE(IsLocalhost("foo.localhoste"));
  EXPECT_FALSE(IsLocalhost("foo.localhos"));
}

}  // namespace net
