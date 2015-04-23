// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/log/net_log_capture_mode.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

TEST(NetLogCaptureMode, None) {
  NetLogCaptureMode mode = NetLogCaptureMode::None();

  EXPECT_FALSE(mode.enabled());
  EXPECT_FALSE(mode.include_cookies_and_credentials());
  EXPECT_FALSE(mode.include_socket_bytes());

  EXPECT_EQ(mode, NetLogCaptureMode::None());
  EXPECT_NE(mode, NetLogCaptureMode::Default());
  EXPECT_NE(mode, NetLogCaptureMode::IncludeCookiesAndCredentials());
  EXPECT_NE(mode, NetLogCaptureMode::IncludeSocketBytes());
  EXPECT_EQ(mode.ToInternalValueForTesting(),
            NetLogCaptureMode::None().ToInternalValueForTesting());
}

TEST(NetLogCaptureMode, Default) {
  NetLogCaptureMode mode = NetLogCaptureMode::Default();

  EXPECT_TRUE(mode.enabled());
  EXPECT_FALSE(mode.include_cookies_and_credentials());
  EXPECT_FALSE(mode.include_socket_bytes());

  EXPECT_NE(mode, NetLogCaptureMode::None());
  EXPECT_EQ(mode, NetLogCaptureMode::Default());
  EXPECT_NE(mode, NetLogCaptureMode::IncludeCookiesAndCredentials());
  EXPECT_NE(mode, NetLogCaptureMode::IncludeSocketBytes());
  EXPECT_EQ(mode.ToInternalValueForTesting(),
            NetLogCaptureMode::Default().ToInternalValueForTesting());
}

TEST(NetLogCaptureMode, IncludeCookiesAndCredentials) {
  NetLogCaptureMode mode = NetLogCaptureMode::IncludeCookiesAndCredentials();

  EXPECT_TRUE(mode.enabled());
  EXPECT_TRUE(mode.include_cookies_and_credentials());
  EXPECT_FALSE(mode.include_socket_bytes());

  EXPECT_NE(mode, NetLogCaptureMode::None());
  EXPECT_NE(mode, NetLogCaptureMode::Default());
  EXPECT_EQ(mode, NetLogCaptureMode::IncludeCookiesAndCredentials());
  EXPECT_NE(mode, NetLogCaptureMode::IncludeSocketBytes());
  EXPECT_EQ(mode.ToInternalValueForTesting(),
            NetLogCaptureMode::IncludeCookiesAndCredentials()
                .ToInternalValueForTesting());
}

TEST(NetLogCaptureMode, IncludeSocketBytes) {
  NetLogCaptureMode mode = NetLogCaptureMode::IncludeSocketBytes();

  EXPECT_TRUE(mode.enabled());
  EXPECT_TRUE(mode.include_cookies_and_credentials());
  EXPECT_TRUE(mode.include_socket_bytes());

  EXPECT_NE(mode, NetLogCaptureMode::None());
  EXPECT_NE(mode, NetLogCaptureMode::Default());
  EXPECT_NE(mode, NetLogCaptureMode::IncludeCookiesAndCredentials());
  EXPECT_EQ(mode, NetLogCaptureMode::IncludeSocketBytes());
  EXPECT_EQ(
      mode.ToInternalValueForTesting(),
      NetLogCaptureMode::IncludeSocketBytes().ToInternalValueForTesting());
}

TEST(NetLogCaptureMode, Max) {
  NetLogCaptureMode none = NetLogCaptureMode::None();
  NetLogCaptureMode all = NetLogCaptureMode::IncludeSocketBytes();
  NetLogCaptureMode cookies = NetLogCaptureMode::IncludeCookiesAndCredentials();
  NetLogCaptureMode def = NetLogCaptureMode::Default();

  EXPECT_EQ(all, NetLogCaptureMode::Max(none, all));
  EXPECT_EQ(all, NetLogCaptureMode::Max(all, none));

  EXPECT_EQ(cookies, NetLogCaptureMode::Max(def, cookies));
  EXPECT_EQ(cookies, NetLogCaptureMode::Max(cookies, def));

  EXPECT_EQ(all, NetLogCaptureMode::Max(def, all));
  EXPECT_EQ(all, NetLogCaptureMode::Max(all, def));
}

}  // namespace

}  // namespace net
