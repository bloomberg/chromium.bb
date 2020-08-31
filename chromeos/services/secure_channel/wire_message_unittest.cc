// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/wire_message.h"

#include <stdint.h>

#include "base/strings/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace secure_channel {

class SecureChannelWireMessageTest : public testing::Test {
 protected:
  SecureChannelWireMessageTest() {}
  ~SecureChannelWireMessageTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SecureChannelWireMessageTest);
};

TEST(SecureChannelWireMessageTest, Deserialize_EmptyMessage) {
  bool is_incomplete;
  std::unique_ptr<WireMessage> message =
      WireMessage::Deserialize(std::string(), &is_incomplete);
  EXPECT_TRUE(is_incomplete);
  EXPECT_FALSE(message);
}

TEST(SecureChannelWireMessageTest, Deserialize_IncompleteHeader) {
  bool is_incomplete;
  std::unique_ptr<WireMessage> message =
      WireMessage::Deserialize("\3", &is_incomplete);
  EXPECT_TRUE(is_incomplete);
  EXPECT_FALSE(message);
}

TEST(SecureChannelWireMessageTest, Deserialize_UnexpectedMessageFormatVersion) {
  bool is_incomplete;
  // Version 2 is below the minimum supported version.
  std::unique_ptr<WireMessage> message =
      WireMessage::Deserialize("\2\1\1", &is_incomplete);
  EXPECT_FALSE(is_incomplete);
  EXPECT_FALSE(message);
}

TEST(SecureChannelWireMessageTest, Deserialize_BodyOfSizeZero) {
  bool is_incomplete;
  std::unique_ptr<WireMessage> message =
      WireMessage::Deserialize(std::string("\3\0\0", 3), &is_incomplete);
  EXPECT_FALSE(is_incomplete);
  EXPECT_FALSE(message);
}

TEST(SecureChannelWireMessageTest, Deserialize_IncompleteBody) {
  bool is_incomplete;
  std::unique_ptr<WireMessage> message =
      WireMessage::Deserialize(std::string("\3\0\5", 3), &is_incomplete);
  EXPECT_TRUE(is_incomplete);
  EXPECT_FALSE(message);
}

TEST(SecureChannelWireMessageTest,
     Deserialize_BodyLongerThanSpecifiedInHeader) {
  bool is_incomplete;
  std::unique_ptr<WireMessage> message = WireMessage::Deserialize(
      std::string("\3\0\5", 3) + "123456", &is_incomplete);
  EXPECT_FALSE(is_incomplete);
  EXPECT_FALSE(message);
}

TEST(SecureChannelWireMessageTest, Deserialize_BodyIsNotValidJSON) {
  bool is_incomplete;
  std::unique_ptr<WireMessage> message = WireMessage::Deserialize(
      std::string("\3\0\5", 3) + "12345", &is_incomplete);
  EXPECT_FALSE(is_incomplete);
  EXPECT_FALSE(message);
}

TEST(SecureChannelWireMessageTest, Deserialize_BodyIsNotADictionary) {
  bool is_incomplete;
  std::string header("\3\0\x15", 3);
  std::string bytes = header + "[{\"payload\": \"YQ==\"}]";
  std::unique_ptr<WireMessage> message =
      WireMessage::Deserialize(bytes, &is_incomplete);
  EXPECT_FALSE(is_incomplete);
  EXPECT_FALSE(message);
}

TEST(SecureChannelWireMessageTest, Deserialize_NonEncryptedMessage) {
  bool is_incomplete;
  std::string header("\3\0\x02", 3);
  std::string bytes = header + "{}";
  std::unique_ptr<WireMessage> message =
      WireMessage::Deserialize(bytes, &is_incomplete);
  EXPECT_FALSE(is_incomplete);
  ASSERT_TRUE(message);
  EXPECT_EQ("{}", message->body());
}

TEST(SecureChannelWireMessageTest, Deserialize_BodyHasEmptyPayload) {
  bool is_incomplete;
  std::string header("\3\0\x29", 3);
  std::string bytes =
      header + "{\"payload\": \"\", \"feature\": \"testFeature\"}";
  std::unique_ptr<WireMessage> message =
      WireMessage::Deserialize(bytes, &is_incomplete);
  EXPECT_FALSE(is_incomplete);
  EXPECT_FALSE(message);
}

TEST(SecureChannelWireMessageTest, Deserialize_PayloadIsNotBase64Encoded) {
  bool is_incomplete;
  std::string header("\3\0\x30", 3);
  std::string bytes =
      header + "{\"payload\": \"garbage\", \"feature\": \"testFeature\"}";
  std::unique_ptr<WireMessage> message =
      WireMessage::Deserialize(bytes, &is_incomplete);
  EXPECT_FALSE(is_incomplete);
  EXPECT_FALSE(message);
}

TEST(SecureChannelWireMessageTest, Deserialize_ValidMessage) {
  bool is_incomplete;
  std::string header("\3\0\x2d", 3);
  std::string bytes =
      header + "{\"payload\": \"YQ==\", \"feature\": \"testFeature\"}";
  std::unique_ptr<WireMessage> message =
      WireMessage::Deserialize(bytes, &is_incomplete);
  EXPECT_FALSE(is_incomplete);
  ASSERT_TRUE(message);
  EXPECT_EQ("a", message->payload());
  EXPECT_EQ("testFeature", message->feature());
}

TEST(SecureChannelWireMessageTest,
     Deserialize_ValidMessageWithBase64UrlEncoding) {
  bool is_incomplete;
  std::string header("\3\0\x2d", 3);
  std::string bytes =
      header + "{\"payload\": \"_-Y=\", \"feature\": \"testFeature\"}";
  std::unique_ptr<WireMessage> message =
      WireMessage::Deserialize(bytes, &is_incomplete);
  EXPECT_FALSE(is_incomplete);
  ASSERT_TRUE(message);
  EXPECT_EQ("\xFF\xE6", message->payload());
  EXPECT_EQ("testFeature", message->feature());
}

TEST(SecureChannelWireMessageTest,
     Deserialize_ValidMessageWithExtraUnknownFields) {
  bool is_incomplete;
  std::string header("\3\0\x4c", 3);
  std::string bytes = header +
                      "{"
                      "  \"payload\": \"YQ==\","
                      "  \"feature\": \"testFeature\","
                      "  \"unexpected\": \"surprise!\""
                      "}";
  std::unique_ptr<WireMessage> message =
      WireMessage::Deserialize(bytes, &is_incomplete);
  EXPECT_FALSE(is_incomplete);
  ASSERT_TRUE(message);
  EXPECT_EQ("a", message->payload());
  EXPECT_EQ("testFeature", message->feature());
}

TEST(SecureChannelWireMessageTest, Deserialize_SizeEquals0x01FF) {
  // Create a message with a body of 0x01FF bytes to test the size contained in
  // the header is parsed correctly.
  std::string header("\3\x01\xff", 3);
  char json_template[] =
      "{"
      "  \"payload\":\"YQ==\","
      "  \"feature\": \"testFeature\","
      "  \"filler\":\"$1\""
      "}";
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
  EXPECT_EQ("testFeature", message->feature());
}

TEST(SecureChannelWireMessageTest, Serialize_WithoutFeature) {
  bool is_incomplete;
  std::string header("\3\0\x13", 3);
  std::string bytes = header + "{\"payload\": \"YQ==\"}";
  std::unique_ptr<WireMessage> message =
      WireMessage::Deserialize(bytes, &is_incomplete);
  EXPECT_FALSE(is_incomplete);
  EXPECT_TRUE(message);
  EXPECT_EQ("a", message->payload());

  // If unspecified, the default feature is "easy_unlock" for backwards
  // compatibility.
  EXPECT_EQ("easy_unlock", message->feature());
}

TEST(SecureChannelWireMessageTest, Serialize_FailsWithoutPayload) {
  WireMessage message1(std::string(), "example id");
  std::string bytes = message1.Serialize();
  EXPECT_TRUE(bytes.empty());
}

}  // namespace secure_channel

}  // namespace chromeos
