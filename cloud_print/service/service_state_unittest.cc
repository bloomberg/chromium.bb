// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/service/service_state.h"

#include "base/strings/string_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Exactly;
using ::testing::Return;

TEST(ServiceStateTest, Empty) {
  ServiceState state;
  EXPECT_FALSE(state.IsValid());
}

TEST(ServiceStateTest, ToString) {
  ServiceState state;
  EXPECT_STREQ("{\"cloud_print\": {\"enabled\": true}}",
               base::CollapseWhitespaceASCII(state.ToString(), true).c_str());
  state.set_email("test@gmail.com");
  state.set_proxy_id("proxy");
  state.set_robot_email("robot@gmail.com");
  state.set_robot_token("abc");
  state.set_auth_token("token1");
  state.set_xmpp_auth_token("token2");
  EXPECT_TRUE(state.IsValid());
  EXPECT_STREQ("{\"cloud_print\": {\"auth_token\": \"token1\",\"email\": "
               "\"test@gmail.com\",\"enabled\": true,\"proxy_id\": \"proxy\","
               "\"robot_email\": \"robot@gmail.com\",\"robot_refresh_token\": "
               "\"abc\",\"xmpp_auth_token\": \"token2\"}}",
               base::CollapseWhitespaceASCII(state.ToString(), true).c_str());
}

TEST(ServiceStateTest, FromString) {
  ServiceState state;
  // Syntax error.
  EXPECT_FALSE(state.FromString("<\"cloud_print\": {\"enabled\": true}}"));
  // No data.
  EXPECT_FALSE(state.FromString("{\"cloud_print\": {\"enabled\": true}}"));
  EXPECT_FALSE(state.FromString(
      "{\"cloud_print\": {\"email\": \"test@gmail.com\"}}"));
  EXPECT_STREQ("test@gmail.com", state.email().c_str());

  // Good state.
  EXPECT_TRUE(state.FromString(
      "{\"cloud_print\": {\"email\": \"test2@gmail.com\",\"enabled\": true,\""
      "proxy_id\": \"proxy\",\"robot_email\": \"robot@gmail.com\",\""
      "robot_refresh_token\": \"abc\"}}"));
  EXPECT_STREQ("test2@gmail.com", state.email().c_str());
  EXPECT_STREQ("proxy", state.proxy_id().c_str());
  EXPECT_STREQ("robot@gmail.com", state.robot_email().c_str());
  EXPECT_STREQ("abc", state.robot_token().c_str());
}

class ServiceStateMock : public ServiceState {
 public:
  ServiceStateMock() {}

  MOCK_METHOD3(LoginToGoogle,
               std::string(const std::string& service,
                           const std::string& email,
                           const std::string& password));

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceStateMock);
};

TEST(ServiceStateTest, Configure) {
  ServiceStateMock state;
  state.set_email("test1@gmail.com");
  state.set_proxy_id("id1");

  EXPECT_CALL(state, LoginToGoogle("cloudprint", "test2@gmail.com", "abc"))
    .Times(Exactly(1))
    .WillOnce(Return("auth1"));
  EXPECT_CALL(state, LoginToGoogle("chromiumsync", "test2@gmail.com", "abc"))
    .Times(Exactly(1))
    .WillOnce(Return("auth2"));

  EXPECT_TRUE(state.Configure("test2@gmail.com", "abc", "id2"));

  EXPECT_STREQ("id2", state.proxy_id().c_str());
  EXPECT_STREQ("auth1", state.auth_token().c_str());
  EXPECT_STREQ("auth2", state.xmpp_auth_token().c_str());
}
