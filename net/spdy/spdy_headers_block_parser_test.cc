// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_headers_block_parser.h"

#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/sys_byteorder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

using base::IntToString;
using base::StringPiece;
using std::string;
using testing::ElementsAre;
using testing::ElementsAreArray;

// A mock the handler class to check that we parse out the correct headers
// and call the callback methods when we should.
class MockSpdyHeadersHandler : public SpdyHeadersHandlerInterface {
 public:
  MOCK_METHOD1(OnHeaderBlock, void(uint32_t num_of_headers));
  MOCK_METHOD0(OnHeaderBlockEnd, void());
  MOCK_METHOD2(OnKeyValuePair, void(const StringPiece&, const StringPiece&));
};

class SpdyHeadersBlockParserTest : public testing::Test {
 public:
  virtual ~SpdyHeadersBlockParserTest() {}

 protected:
  virtual void SetUp() {
    // Create a parser using the mock handler.
    parser_.reset(new SpdyHeadersBlockParser(&handler_));
  }

  MockSpdyHeadersHandler handler_;
  scoped_ptr<SpdyHeadersBlockParser> parser_;

  // Create a header block with a specified number of headers.
  static string CreateHeaders(uint32_t num_headers, bool insert_nulls) {
    string headers;
    char length_field[sizeof(uint32_t)];

    // First, write the number of headers in the header block.
    StoreInt(num_headers, length_field);
    headers += string(length_field, sizeof(length_field));

    // Second, write the key-value pairs.
    for (uint32_t i = 0; i < num_headers; i++) {
      // Build the key.
      string key;
      if (insert_nulls) {
        key = string(base_key) + string("\0", 1) + IntToString(i);
      } else {
        key = string(base_key) + IntToString(i);
      }
      // Encode the key as SPDY header.
      StoreInt(key.length(), length_field);
      headers += string(length_field, sizeof(length_field));
      headers += key;

      // Build the value.
      string value;
      if (insert_nulls) {
        value = string(base_value) + string("\0", 1) + IntToString(i);
      } else {
        value = string(base_value) + IntToString(i);
      }
      // Encode the value as SPDY header.
      StoreInt(value.length(), length_field);
      headers += string(length_field, sizeof(length_field));
      headers += value;
    }

    return headers;
  }

  static const char* base_key;
  static const char* base_value;

  // Number of headers and header blocks used in the tests.
  static const int kNumHeadersInBlock = 10;
  static const int kNumHeaderBlocks = 10;

 private:
  static void StoreInt(uint32_t num, char* buf) {
    uint32_t net_order_len = base::HostToNet32(num);
    memcpy(buf, &net_order_len, sizeof(num));
  }
};

const char* SpdyHeadersBlockParserTest::base_key = "test_key";
const char* SpdyHeadersBlockParserTest::base_value = "test_value";

TEST_F(SpdyHeadersBlockParserTest, Basic) {
  // Sanity test, verify that we parse out correctly a block with
  // a single key-value pair and that we notify when we start and finish
  // handling a headers block.
  EXPECT_CALL(handler_, OnHeaderBlock(1)).Times(1);

  std::string expect_key = base_key + IntToString(0);
  std::string expect_value = base_value + IntToString(0);
  EXPECT_CALL(handler_, OnKeyValuePair(StringPiece(expect_key),
                                       StringPiece(expect_value))).Times(1);
  EXPECT_CALL(handler_, OnHeaderBlockEnd()).Times(1);

  string headers(CreateHeaders(1, false));
  parser_->HandleControlFrameHeadersData(headers.c_str(), headers.length());
}

TEST_F(SpdyHeadersBlockParserTest, NullsSupported) {
  // Sanity test, verify that we parse out correctly a block with
  // a single key-value pair when the key and value contain null charecters.
  EXPECT_CALL(handler_, OnHeaderBlock(1)).Times(1);

  std::string expect_key = base_key + string("\0", 1) + IntToString(0);
  std::string expect_value = base_value + string("\0", 1) + IntToString(0);
  EXPECT_CALL(handler_, OnKeyValuePair(StringPiece(expect_key),
                                       StringPiece(expect_value))).Times(1);
  EXPECT_CALL(handler_, OnHeaderBlockEnd()).Times(1);

  string headers(CreateHeaders(1, true));
  parser_->HandleControlFrameHeadersData(headers.c_str(), headers.length());
}

TEST_F(SpdyHeadersBlockParserTest, MultipleBlocksMultipleHeadersPerBlock) {
  testing::InSequence s;

  // The mock doesn't retain storage of arguments, so keep them in scope.
  std::vector<string> retained_arguments;
  for (int i = 0; i < kNumHeadersInBlock; i++) {
    retained_arguments.push_back(base_key + IntToString(i));
    retained_arguments.push_back(base_value + IntToString(i));
  }
  // For each block we expect to parse out the headers in order.
  for (int i = 0; i < kNumHeaderBlocks; i++) {
    EXPECT_CALL(handler_, OnHeaderBlock(kNumHeadersInBlock)).Times(1);
    for (int j = 0; j < kNumHeadersInBlock; j++) {
      EXPECT_CALL(handler_, OnKeyValuePair(
          StringPiece(retained_arguments[2 * j]),
          StringPiece(retained_arguments[2 * j + 1]))).Times(1);
    }
    EXPECT_CALL(handler_, OnHeaderBlockEnd()).Times(1);
  }
  // Parse the blocks, breaking them into multiple reads at various points.
  for (int i = 0; i < kNumHeaderBlocks; i++) {
    unsigned break_index = 4 * i;
    string headers(CreateHeaders(kNumHeadersInBlock, false));
    parser_->HandleControlFrameHeadersData(
        headers.c_str(), headers.length() - break_index);
    parser_->HandleControlFrameHeadersData(
        headers.c_str() + headers.length() - break_index, break_index);
  }
}

TEST(SpdyHeadersBlockParserReader, EmptyPrefix) {
  std::string prefix = "";
  std::string suffix = "foobar";
  char buffer[] = "12345";

  SpdyHeadersBlockParserReader reader(prefix, suffix);
  EXPECT_EQ(6u, reader.Available());
  EXPECT_FALSE(reader.ReadN(10, buffer));  // Not enough buffer.
  EXPECT_TRUE(reader.ReadN(5, buffer));
  EXPECT_THAT(buffer, ElementsAreArray("fooba"));
  EXPECT_EQ(1u, reader.Available());
  EXPECT_THAT(reader.Remainder(), ElementsAre('r'));
}

TEST(SpdyHeadersBlockParserReader, EmptySuffix) {
  std::string prefix = "foobar";
  std::string suffix = "";
  char buffer[] = "12345";

  SpdyHeadersBlockParserReader reader(prefix, suffix);
  EXPECT_EQ(6u, reader.Available());
  EXPECT_FALSE(reader.ReadN(10, buffer));  // Not enough buffer.
  EXPECT_TRUE(reader.ReadN(5, buffer));
  EXPECT_THAT(buffer, ElementsAreArray("fooba"));
  EXPECT_EQ(1u, reader.Available());
  EXPECT_THAT(reader.Remainder(), ElementsAre('r'));
}

TEST(SpdyHeadersBlockParserReader, ReadInPrefix) {
  std::string prefix = "abcdef";
  std::string suffix = "ghijk";
  char buffer[] = "1234";

  SpdyHeadersBlockParserReader reader(prefix, suffix);
  EXPECT_EQ(11u, reader.Available());
  EXPECT_TRUE(reader.ReadN(4, buffer));
  EXPECT_THAT(buffer, ElementsAreArray("abcd"));
  EXPECT_EQ(7u, reader.Available());
  EXPECT_THAT(reader.Remainder(),
            ElementsAre('e', 'f', 'g', 'h', 'i', 'j', 'k'));
}

TEST(SpdyHeadersBlockParserReader, ReadPastPrefix) {
  std::string prefix = "abcdef";
  std::string suffix = "ghijk";
  char buffer[] = "12345";

  SpdyHeadersBlockParserReader reader(prefix, suffix);
  EXPECT_EQ(11u, reader.Available());
  EXPECT_TRUE(reader.ReadN(5, buffer));
  EXPECT_THAT(buffer, ElementsAreArray("abcde"));

  // One byte of prefix left: read it.
  EXPECT_EQ(6u, reader.Available());
  EXPECT_TRUE(reader.ReadN(1, buffer));
  EXPECT_THAT(buffer, ElementsAreArray("fbcde"));

  // Read one byte of suffix.
  EXPECT_EQ(5u, reader.Available());
  EXPECT_TRUE(reader.ReadN(1, buffer));
  EXPECT_THAT(buffer, ElementsAreArray("gbcde"));

  EXPECT_THAT(reader.Remainder(),
            ElementsAre('h', 'i', 'j', 'k'));
}

TEST(SpdyHeadersBlockParserReader, ReadAcrossSuffix) {
  std::string prefix = "abcdef";
  std::string suffix = "ghijk";
  char buffer[] = "12345678";

  SpdyHeadersBlockParserReader reader(prefix, suffix);
  EXPECT_EQ(11u, reader.Available());
  EXPECT_TRUE(reader.ReadN(8, buffer));
  EXPECT_THAT(buffer, ElementsAreArray("abcdefgh"));

  EXPECT_EQ(3u, reader.Available());
  EXPECT_FALSE(reader.ReadN(4, buffer));
  EXPECT_TRUE(reader.ReadN(3, buffer));
  EXPECT_THAT(buffer, ElementsAreArray("ijkdefgh"));

  EXPECT_EQ(0u, reader.Available());
  EXPECT_THAT(reader.Remainder(), ElementsAre());
}

}  // namespace net
