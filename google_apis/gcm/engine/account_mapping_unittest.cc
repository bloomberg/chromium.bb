// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/account_mapping.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {

TEST(AccountMappingTest, SerializeAccountMapping) {
  AccountMapping account_mapping;
  account_mapping.account_id = "acc_id";
  account_mapping.email = "test@example.com";
  account_mapping.access_token = "access_token";
  account_mapping.status = AccountMapping::ADDING;
  account_mapping.status_change_timestamp = base::Time();
  account_mapping.last_message_id = "last_message_id_1";
  account_mapping.last_message_type = AccountMapping::MSG_ADD;

  EXPECT_EQ("test@example.com&0&add&last_message_id_1",
            account_mapping.SerializeAsString());

  account_mapping.account_id = "acc_id2";
  account_mapping.email = "test@gmail.com";
  account_mapping.access_token = "access_token";  // should be ignored.
  account_mapping.status = AccountMapping::MAPPED;  // should be ignored.
  account_mapping.status_change_timestamp =
      base::Time::FromInternalValue(1305797421259977LL);
  account_mapping.last_message_id = "last_message_id_2";
  account_mapping.last_message_type = AccountMapping::MSG_REMOVE;

  EXPECT_EQ("test@gmail.com&1305797421259977&remove&last_message_id_2",
            account_mapping.SerializeAsString());

  account_mapping.last_message_type = AccountMapping::MSG_NONE;

  EXPECT_EQ("test@gmail.com&1305797421259977&none",
            account_mapping.SerializeAsString());
}

TEST(AccountMappingTest, DeserializeAccountMapping) {
  AccountMapping account_mapping;
  account_mapping.account_id = "acc_id";
  EXPECT_TRUE(account_mapping.ParseFromString(
      "test@example.com&0&add&last_message_id_1"));
  EXPECT_EQ("acc_id", account_mapping.account_id);
  EXPECT_EQ("test@example.com", account_mapping.email);
  EXPECT_TRUE(account_mapping.access_token.empty());
  EXPECT_EQ(AccountMapping::ADDING, account_mapping.status);
  EXPECT_EQ(base::Time(), account_mapping.status_change_timestamp);
  EXPECT_EQ(AccountMapping::MSG_ADD, account_mapping.last_message_type);
  EXPECT_EQ("last_message_id_1", account_mapping.last_message_id);

  EXPECT_TRUE(account_mapping.ParseFromString(
      "test@gmail.com&1305797421259977&remove&last_message_id_2"));
  EXPECT_EQ("acc_id", account_mapping.account_id);
  EXPECT_EQ("test@gmail.com", account_mapping.email);
  EXPECT_TRUE(account_mapping.access_token.empty());
  EXPECT_EQ(AccountMapping::REMOVING, account_mapping.status);
  EXPECT_EQ(base::Time::FromInternalValue(1305797421259977LL),
            account_mapping.status_change_timestamp);
  EXPECT_EQ(AccountMapping::MSG_REMOVE, account_mapping.last_message_type);
  EXPECT_EQ("last_message_id_2", account_mapping.last_message_id);

  EXPECT_TRUE(account_mapping.ParseFromString(
      "test@gmail.com&1305797421259977&none"));
  EXPECT_EQ("acc_id", account_mapping.account_id);
  EXPECT_EQ("test@gmail.com", account_mapping.email);
  EXPECT_TRUE(account_mapping.access_token.empty());
  EXPECT_EQ(AccountMapping::MAPPED, account_mapping.status);
  EXPECT_EQ(base::Time::FromInternalValue(1305797421259977LL),
            account_mapping.status_change_timestamp);
  EXPECT_EQ(AccountMapping::MSG_NONE, account_mapping.last_message_type);
  EXPECT_EQ("", account_mapping.last_message_id);
}

TEST(AccountMappingTest, DeserializeAccountMappingInvalidInput) {
  AccountMapping account_mapping;
  account_mapping.account_id = "acc_id";
  // Too many agruments.
  EXPECT_FALSE(account_mapping.ParseFromString(
      "test@example.com&1305797421259935"
      "&add&last_message_id_1&stuff_here"));
  // Too few arguments.
  EXPECT_FALSE(account_mapping.ParseFromString(
      "test@example.com&1305797421259935&remove"));
  // Too few arguments.
  EXPECT_FALSE(account_mapping.ParseFromString(
      "test@example.com&1305797421259935"));
  // Missing email.
  EXPECT_FALSE(account_mapping.ParseFromString(
      "&1305797421259935&remove&last_message_id_2"));
  // Missing mapping status change timestamp.
  EXPECT_FALSE(account_mapping.ParseFromString(
      "test@gmail.com&&remove&last_message_id_2"));
  // Last mapping status change timestamp not parseable.
  EXPECT_FALSE(account_mapping.ParseFromString(
      "test@gmail.com&remove&asdfjkl&last_message_id_2"));
  // Missing message type.
  EXPECT_FALSE(account_mapping.ParseFromString(
      "test@example.com&1305797421259935&&last_message_id_2"));
  // Unkown message type.
  EXPECT_FALSE(account_mapping.ParseFromString(
      "test@example.com&1305797421259935&random&last_message_id_2"));
  // Message type is none when message details specified.
  EXPECT_FALSE(account_mapping.ParseFromString(
      "test@example.com&1305797421259935&none&last_message_id_2"));
  // Message type is messed up.
  EXPECT_FALSE(account_mapping.ParseFromString(
      "test@example.com&1305797421259935&random"));
  // Missing last message ID.
  EXPECT_FALSE(account_mapping.ParseFromString(
      "test@example.com&1305797421259935&remove&"));
}

}  // namespace
}  // namespace gcm
