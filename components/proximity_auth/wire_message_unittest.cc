// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/wire_message.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace proximity_auth {

TEST(ProximityAuthWireMessage, Deserialize_EmptyMessage) {
  bool is_incomplete;
  scoped_ptr<WireMessage> message =
      WireMessage::Deserialize(std::string(), &is_incomplete);
  EXPECT_TRUE(is_incomplete);
  EXPECT_FALSE(message);
}

TEST(ProximityAuthWireMessage, Deserialize_IncompleteHeader) {
  bool is_incomplete;
  scoped_ptr<WireMessage> message =
      WireMessage::Deserialize("\3", &is_incomplete);
  EXPECT_TRUE(is_incomplete);
  EXPECT_FALSE(message);
}

TEST(ProximityAuthWireMessage, Deserialize_UnexpectedMessageFormatVersion) {
  bool is_incomplete;
  // Version 2 is below the minimum supported version.
  scoped_ptr<WireMessage> message =
      WireMessage::Deserialize("\2\1\1", &is_incomplete);
  EXPECT_FALSE(is_incomplete);
  EXPECT_FALSE(message);
}

TEST(ProximityAuthWireMessage, Deserialize_BodyOfSizeZero) {
  bool is_incomplete;
  scoped_ptr<WireMessage> message =
      WireMessage::Deserialize(std::string("\3\0\0", 3), &is_incomplete);
  EXPECT_FALSE(is_incomplete);
  EXPECT_FALSE(message);
}

TEST(ProximityAuthWireMessage, Deserialize_IncompleteBody) {
  bool is_incomplete;
  scoped_ptr<WireMessage> message =
      WireMessage::Deserialize(std::string("\3\0\5", 3), &is_incomplete);
  EXPECT_TRUE(is_incomplete);
  EXPECT_FALSE(message);
}

TEST(ProximityAuthWireMessage, Deserialize_BodyLongerThanSpecifiedInHeader) {
  bool is_incomplete;
  scoped_ptr<WireMessage> message = WireMessage::Deserialize(
      std::string("\3\0\5", 3) + "123456", &is_incomplete);
  EXPECT_FALSE(is_incomplete);
  EXPECT_FALSE(message);
}

TEST(ProximityAuthWireMessage, Deserialize_BodyIsNotValidJSON) {
  bool is_incomplete;
  scoped_ptr<WireMessage> message = WireMessage::Deserialize(
      std::string("\3\0\5", 3) + "12345", &is_incomplete);
  EXPECT_FALSE(is_incomplete);
  EXPECT_FALSE(message);
}

TEST(ProximityAuthWireMessage, Deserialize_BodyIsNotADictionary) {
  bool is_incomplete;
  std::string header("\3\0\x29", 3);
  std::string bytes =
      header + "[{\"permit_id\": \"Hi!\", \"payload\": \"YQ==\"}]";
  scoped_ptr<WireMessage> message =
      WireMessage::Deserialize(bytes, &is_incomplete);
  EXPECT_FALSE(is_incomplete);
  EXPECT_FALSE(message);
}

// The permit ID is optional.
TEST(ProximityAuthWireMessage, Deserialize_BodyLacksPermitId) {
  bool is_incomplete;
  std::string header("\3\0\x13", 3);
  std::string bytes = header + "{\"payload\": \"YQ==\"}";
  scoped_ptr<WireMessage> message =
      WireMessage::Deserialize(bytes, &is_incomplete);
  EXPECT_FALSE(is_incomplete);
  EXPECT_TRUE(message);
  EXPECT_EQ(std::string(), message->permit_id());
  EXPECT_EQ("a", message->payload());
}

TEST(ProximityAuthWireMessage, Deserialize_BodyLacksPayload) {
  bool is_incomplete;
  std::string header("\3\0\x14", 3);
  std::string bytes = header + "{\"permit_id\": \"Hi!\"}";
  scoped_ptr<WireMessage> message =
      WireMessage::Deserialize(bytes, &is_incomplete);
  EXPECT_FALSE(is_incomplete);
  EXPECT_FALSE(message);
}

// The permit ID is optional.
TEST(ProximityAuthWireMessage, Deserialize_BodyHasEmptyPermitId) {
  bool is_incomplete;
  std::string header("\3\0\x24", 3);
  std::string bytes = header + "{\"permit_id\": \"\", \"payload\": \"YQ==\"}";
  scoped_ptr<WireMessage> message =
      WireMessage::Deserialize(bytes, &is_incomplete);
  EXPECT_FALSE(is_incomplete);
  EXPECT_TRUE(message);
  EXPECT_EQ(std::string(), message->permit_id());
  EXPECT_EQ("a", message->payload());
}

TEST(ProximityAuthWireMessage, Deserialize_BodyHasEmptyPayload) {
  bool is_incomplete;
  std::string header("\3\0\x23", 3);
  std::string bytes = header + "{\"permit_id\": \"Hi!\", \"payload\": \"\"}";
  scoped_ptr<WireMessage> message =
      WireMessage::Deserialize(bytes, &is_incomplete);
  EXPECT_FALSE(is_incomplete);
  EXPECT_FALSE(message);
}

TEST(ProximityAuthWireMessage, Deserialize_PayloadIsNotBase64Encoded) {
  bool is_incomplete;
  std::string header("\3\0\x2A", 3);
  std::string bytes =
      header + "{\"permit_id\": \"Hi!\", \"payload\": \"garbage\"}";
  scoped_ptr<WireMessage> message =
      WireMessage::Deserialize(bytes, &is_incomplete);
  EXPECT_FALSE(is_incomplete);
  EXPECT_FALSE(message);
}

TEST(ProximityAuthWireMessage, Deserialize_ValidMessage) {
  bool is_incomplete;
  std::string header("\3\0\x27", 3);
  std::string bytes =
      header + "{\"permit_id\": \"Hi!\", \"payload\": \"YQ==\"}";
  scoped_ptr<WireMessage> message =
      WireMessage::Deserialize(bytes, &is_incomplete);
  EXPECT_FALSE(is_incomplete);
  EXPECT_TRUE(message);
  EXPECT_EQ("Hi!", message->permit_id());
  EXPECT_EQ("a", message->payload());
}

TEST(ProximityAuthWireMessage, Deserialize_ValidMessageWithExtraUnknownFields) {
  bool is_incomplete;
  std::string header("\3\0\x46", 3);
  std::string bytes = header +
                      "{"
                      "  \"permit_id\": \"Hi!\","
                      "  \"payload\": \"YQ==\","
                      "  \"unexpected\": \"surprise!\""
                      "}";
  scoped_ptr<WireMessage> message =
      WireMessage::Deserialize(bytes, &is_incomplete);
  EXPECT_FALSE(is_incomplete);
  EXPECT_TRUE(message);
  EXPECT_EQ("Hi!", message->permit_id());
  EXPECT_EQ("a", message->payload());
}

}  // namespace proximity_auth
