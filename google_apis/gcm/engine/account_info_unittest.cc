// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "google_apis/gcm/engine/account_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {

TEST(AccountInfoTest, SerializeAccountInfo) {
  AccountInfo account_info;
  account_info.account_id = "acc_id";
  account_info.email = "test@example.com";
  account_info.last_message_id = "last_message_id_1";
  account_info.last_message_type = AccountInfo::MSG_ADD;
  account_info.last_message_timestamp = base::Time::FromInternalValue(
      1305797421259935LL);  // Arbitrary timestamp.

  EXPECT_EQ("test@example.com&add&last_message_id_1&1305797421259935",
            account_info.SerializeAsString());

  account_info.account_id = "acc_id2";
  account_info.email = "test@gmail.com";
  account_info.last_message_id = "last_message_id_2";
  account_info.last_message_type = AccountInfo::MSG_REMOVE;
  account_info.last_message_timestamp =
      base::Time::FromInternalValue(1305734521259935LL);  // Other timestamp.

  EXPECT_EQ("test@gmail.com&remove&last_message_id_2&1305734521259935",
            account_info.SerializeAsString());

  account_info.last_message_type = AccountInfo::MSG_NONE;

  EXPECT_EQ("test@gmail.com&none", account_info.SerializeAsString());
}

TEST(AccountInfoTest, DeserializeAccountInfo) {
  AccountInfo account_info;
  account_info.account_id = "acc_id";
  EXPECT_TRUE(account_info.ParseFromString(
      "test@example.com&add&last_message_id_1&1305797421259935"));
  EXPECT_EQ("acc_id", account_info.account_id);
  EXPECT_EQ("test@example.com", account_info.email);
  EXPECT_EQ(AccountInfo::MSG_ADD, account_info.last_message_type);
  EXPECT_EQ("last_message_id_1", account_info.last_message_id);
  EXPECT_EQ(base::Time::FromInternalValue(1305797421259935LL),
            account_info.last_message_timestamp);

  EXPECT_TRUE(account_info.ParseFromString(
      "test@gmail.com&remove&last_message_id_2&1305734521259935"));
  EXPECT_EQ("acc_id", account_info.account_id);
  EXPECT_EQ("test@gmail.com", account_info.email);
  EXPECT_EQ(AccountInfo::MSG_REMOVE, account_info.last_message_type);
  EXPECT_EQ("last_message_id_2", account_info.last_message_id);
  EXPECT_EQ(base::Time::FromInternalValue(1305734521259935LL),
            account_info.last_message_timestamp);

  EXPECT_TRUE(account_info.ParseFromString("test@gmail.com&none"));
  EXPECT_EQ("acc_id", account_info.account_id);
  EXPECT_EQ("test@gmail.com", account_info.email);
  EXPECT_EQ(AccountInfo::MSG_NONE, account_info.last_message_type);
  EXPECT_EQ("", account_info.last_message_id);
  EXPECT_EQ(base::Time(), account_info.last_message_timestamp);
}

TEST(AccountInfoTest, DeserializeAccountInfoInvalidInput) {
  AccountInfo account_info;
  account_info.account_id = "acc_id";
  // Too many agruments.
  EXPECT_FALSE(account_info.ParseFromString(
      "test@example.com&add&last_message_id_1&1305797421259935&stuff_here"));
  // Too few arguments.
  EXPECT_FALSE(account_info.ParseFromString(
      "test@example.com&remove&last_message_id_1"));
  // Too few arguments.
  EXPECT_FALSE(account_info.ParseFromString(
      "test@example.com"));
  // Missing email.
  EXPECT_FALSE(account_info.ParseFromString(
      "&remove&last_message_id_2&1305734521259935"));
  // Missing message type.
  EXPECT_FALSE(account_info.ParseFromString(
      "test@gmail.com&&last_message_id_2&1305734521259935"));
  // Unkown message type.
  EXPECT_FALSE(account_info.ParseFromString(
      "test@gmail.com&random&last_message_id_2&1305734521259935"));
  // Message type is none when message details specified.
  EXPECT_FALSE(account_info.ParseFromString(
      "test@gmail.com&none&last_message_id_2&1305734521259935"));
  // Message type is messed up, but we have no message -- that's OK.
  EXPECT_TRUE(account_info.ParseFromString(
      "test@gmail.com&random"));
  // Missing last message ID.
  EXPECT_FALSE(account_info.ParseFromString(
      "test@gmail.com&remove&&1305734521259935"));
  // Missing last message timestamp.
  EXPECT_FALSE(account_info.ParseFromString(
      "test@gmail.com&remove&last_message_id&"));
  // Last message timestamp not parseable.
  EXPECT_FALSE(account_info.ParseFromString(
      "test@gmail.com&remove&last_message_id&asdfjlk"));
}

}  // namespace
}  // namespace gcm
