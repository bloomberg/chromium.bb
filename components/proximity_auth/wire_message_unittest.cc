// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/wire_message.h"

#include <stdint.h>

#include "base/strings/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace proximity_auth {

TEST(ProximityAuthWireMessage, Deserialize_EmptyMessage) {
  bool is_incomplete;
  std::unique_ptr<WireMessage> message =
      WireMessage::Deserialize(std::string(), &is_incomplete);
  EXPECT_TRUE(is_incomplete);
  EXPECT_FALSE(message);
}

TEST(ProximityAuthWireMessage, Deserialize_IncompleteHeader) {
  bool is_incomplete;
  std::unique_ptr<WireMessage> message =
      WireMessage::Deserialize("\3", &is_incomplete);
  EXPECT_TRUE(is_incomplete);
  EXPECT_FALSE(message);
}

TEST(ProximityAuthWireMessage, Deserialize_UnexpectedMessageFormatVersion) {
  bool is_incomplete;
  // Version 2 is below the minimum supported version.
  std::unique_ptr<WireMessage> message =
      WireMessage::Deserialize("\2\1\1", &is_incomplete);
  EXPECT_FALSE(is_incomplete);
  EXPECT_FALSE(message);
}

TEST(ProximityAuthWireMessage, Deserialize_BodyOfSizeZero) {
  bool is_incomplete;
  std::unique_ptr<WireMessage> message =
      WireMessage::Deserialize(std::string("\3\0\0", 3), &is_incomplete);
  EXPECT_FALSE(is_incomplete);
  EXPECT_FALSE(message);
}

TEST(ProximityAuthWireMessage, Deserialize_IncompleteBody) {
  bool is_incomplete;
  std::unique_ptr<WireMessage> message =
      WireMessage::Deserialize(std::string("\3\0\5", 3), &is_incomplete);
  EXPECT_TRUE(is_incomplete);
  EXPECT_FALSE(message);
}

TEST(ProximityAuthWireMessage, Deserialize_BodyLongerThanSpecifiedInHeader) {
  bool is_incomplete;
  std::unique_ptr<WireMessage> message = WireMessage::Deserialize(
      std::string("\3\0\5", 3) + "123456", &is_incomplete);
  EXPECT_FALSE(is_incomplete);
  EXPECT_FALSE(message);
}

TEST(ProximityAuthWireMessage, Deserialize_BodyIsNotValidJSON) {
  bool is_incomplete;
  std::unique_ptr<WireMessage> message = WireMessage::Deserialize(
      std::string("\3\0\5", 3) + "12345", &is_incomplete);
  EXPECT_FALSE(is_incomplete);
  EXPECT_FALSE(message);
}

TEST(ProximityAuthWireMessage, Deserialize_BodyIsNotADictionary) {
  bool is_incomplete;
  std::string header("\3\0\x29", 3);
  std::string bytes =
      header + "[{\"permit_id\": \"Hi!\", \"payload\": \"YQ==\"}]";
  std::unique_ptr<WireMessage> message =
      WireMessage::Deserialize(bytes, &is_incomplete);
  EXPECT_FALSE(is_incomplete);
  EXPECT_FALSE(message);
}

// The permit ID is optional.
TEST(ProximityAuthWireMessage, Deserialize_BodyLacksPermitId) {
  bool is_incomplete;
  std::string header("\3\0\x13", 3);
  std::string bytes = header + "{\"payload\": \"YQ==\"}";
  std::unique_ptr<WireMessage> message =
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
  std::unique_ptr<WireMessage> message =
      WireMessage::Deserialize(bytes, &is_incomplete);
  EXPECT_FALSE(is_incomplete);
  EXPECT_FALSE(message);
}

// The permit ID is optional.
TEST(ProximityAuthWireMessage, Deserialize_BodyHasEmptyPermitId) {
  bool is_incomplete;
  std::string header("\3\0\x24", 3);
  std::string bytes = header + "{\"permit_id\": \"\", \"payload\": \"YQ==\"}";
  std::unique_ptr<WireMessage> message =
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
  std::unique_ptr<WireMessage> message =
      WireMessage::Deserialize(bytes, &is_incomplete);
  EXPECT_FALSE(is_incomplete);
  EXPECT_FALSE(message);
}

TEST(ProximityAuthWireMessage, Deserialize_PayloadIsNotBase64Encoded) {
  bool is_incomplete;
  std::string header("\3\0\x2A", 3);
  std::string bytes =
      header + "{\"permit_id\": \"Hi!\", \"payload\": \"garbage\"}";
  std::unique_ptr<WireMessage> message =
      WireMessage::Deserialize(bytes, &is_incomplete);
  EXPECT_FALSE(is_incomplete);
  EXPECT_FALSE(message);
}

TEST(ProximityAuthWireMessage, Deserialize_ValidMessage) {
  bool is_incomplete;
  std::string header("\3\0\x27", 3);
  std::string bytes =
      header + "{\"permit_id\": \"Hi!\", \"payload\": \"YQ==\"}";
  std::unique_ptr<WireMessage> message =
      WireMessage::Deserialize(bytes, &is_incomplete);
  EXPECT_FALSE(is_incomplete);
  ASSERT_TRUE(message);
  EXPECT_EQ("Hi!", message->permit_id());
  EXPECT_EQ("a", message->payload());
}

TEST(ProximityAuthWireMessage, Deserialize_ValidMessageWithBase64UrlEncoding) {
  bool is_incomplete;
  std::string header("\3\0\x27", 3);
  std::string bytes =
      header + "{\"permit_id\": \"Hi!\", \"payload\": \"_-Y=\"}";
  std::unique_ptr<WireMessage> message =
      WireMessage::Deserialize(bytes, &is_incomplete);
  EXPECT_FALSE(is_incomplete);
  ASSERT_TRUE(message);
  EXPECT_EQ("Hi!", message->permit_id());
  EXPECT_EQ("\xFF\xE6", message->payload());
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
  std::unique_ptr<WireMessage> message =
      WireMessage::Deserialize(bytes, &is_incomplete);
  EXPECT_FALSE(is_incomplete);
  ASSERT_TRUE(message);
  EXPECT_EQ("Hi!", message->permit_id());
  EXPECT_EQ("a", message->payload());
}

TEST(ProximityAuthWireMessage, Deserialize_SizeEquals0x01FF) {
  // Create a message with a body of 0x01FF bytes to test the size contained in
  // the header is parsed correctly.
  std::string header("\3\x01\xff", 3);
  char json_template[] = "{\"payload\":\"YQ==\", \"filler\":\"$1\"}";
  // Add 3 to the size to take into account the "$1" and NUL terminator ("\0")
  // characters in |json_template|.
  uint16_t filler_size = 0x01ff - sizeof(json_template) + 3;
  std::string filler(filler_size, 'F');

  std::string body = base::ReplaceStringPlaceholders(
      json_template, std::vector<std::string>(1u, filler), nullptr);
  std::string serialized_message = header + body;

  bool is_incomplete;
  std::unique_ptr<WireMessage> message =
      WireMessage::Deserialize(serialized_message, &is_incomplete);
  EXPECT_FALSE(is_incomplete);
  ASSERT_TRUE(message);
  EXPECT_EQ("a", message->payload());
}

TEST(ProximityAuthWireMessage, Serialize_WithPermitId) {
  WireMessage message1("example payload", "example id");
  std::string bytes = message1.Serialize();
  ASSERT_FALSE(bytes.empty());

  bool is_incomplete;
  std::unique_ptr<WireMessage> message2 =
      WireMessage::Deserialize(bytes, &is_incomplete);
  EXPECT_FALSE(is_incomplete);
  ASSERT_TRUE(message2);
  EXPECT_EQ("example id", message2->permit_id());
  EXPECT_EQ("example payload", message2->payload());
}

TEST(ProximityAuthWireMessage, Serialize_WithoutPermitId) {
  WireMessage message1("example payload");
  std::string bytes = message1.Serialize();
  ASSERT_FALSE(bytes.empty());

  bool is_incomplete;
  std::unique_ptr<WireMessage> message2 =
      WireMessage::Deserialize(bytes, &is_incomplete);
  EXPECT_FALSE(is_incomplete);
  ASSERT_TRUE(message2);
  EXPECT_EQ(std::string(), message2->permit_id());
  EXPECT_EQ("example payload", message2->payload());
}

TEST(ProximityAuthWireMessage, Serialize_FailsWithoutPayload) {
  WireMessage message1(std::string(), "example id");
  std::string bytes = message1.Serialize();
  EXPECT_TRUE(bytes.empty());
}

}  // namespace proximity_auth
