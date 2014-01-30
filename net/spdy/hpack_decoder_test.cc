// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/hpack_decoder.h"

#include <map>
#include <string>

#include "base/basictypes.h"
#include "net/spdy/hpack_encoder.h"
#include "net/spdy/hpack_input_stream.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

using base::StringPiece;
using std::string;

// Decoding an encoded name with a valid string literal should work.
TEST(HpackDecoderTest, DecodeNextNameLiteral) {
  HpackDecoder decoder(kuint32max);
  HpackInputStream input_stream(kuint32max, StringPiece("\x00\x04name", 6));

  StringPiece string_piece;
  EXPECT_TRUE(decoder.DecodeNextNameForTest(&input_stream, &string_piece));
  EXPECT_EQ("name", string_piece);
  EXPECT_FALSE(input_stream.HasMoreData());
}

// Decoding an encoded name with a valid index should work.
TEST(HpackDecoderTest, DecodeNextNameIndexed) {
  HpackDecoder decoder(kuint32max);
  HpackInputStream input_stream(kuint32max, "\x01");

  StringPiece string_piece;
  EXPECT_TRUE(decoder.DecodeNextNameForTest(&input_stream, &string_piece));
  EXPECT_EQ(":authority", string_piece);
  EXPECT_FALSE(input_stream.HasMoreData());
}

// Decoding an encoded name with an invalid index should fail.
TEST(HpackDecoderTest, DecodeNextNameInvalidIndex) {
  // One more than the number of static table entries.
  HpackDecoder decoder(kuint32max);
  HpackInputStream input_stream(kuint32max, "\x3d");

  StringPiece string_piece;
  EXPECT_FALSE(decoder.DecodeNextNameForTest(&input_stream, &string_piece));
}

// Decoding two valid encoded literal headers with no indexing should
// work.
TEST(HpackDecoderTest, LiteralHeaderNoIndexing) {
  HpackDecoder decoder(kuint32max);
  HpackHeaderPairVector header_list;
  // First header with indexed name, second header with string literal
  // name.
  EXPECT_TRUE(decoder.DecodeHeaderSet(
      "\x44\x0c/sample/path\x40\x06:path2\x0e/sample/path/2",
      &header_list));
  std::map<string, string> header_set(header_list.begin(), header_list.end());

  std::map<string, string> expected_header_set;
  expected_header_set[":path"] = "/sample/path";
  expected_header_set[":path2"] = "/sample/path/2";
  EXPECT_EQ(expected_header_set, header_set);
}

// Round-tripping the header set from E.2.1 should work.
TEST(HpackDecoderTest, BasicE21) {
  HpackEncoder encoder(kuint32max);

  std::map<string, string> expected_header_set;
  expected_header_set[":method"] = "GET";
  expected_header_set[":scheme"] = "http";
  expected_header_set[":path"] = "/";
  expected_header_set[":authority"] = "www.example.com";

  string encoded_header_set;
  EXPECT_TRUE(encoder.EncodeHeaderSet(
      expected_header_set, &encoded_header_set));

  HpackDecoder decoder(kuint32max);
  HpackHeaderPairVector header_list;
  EXPECT_TRUE(decoder.DecodeHeaderSet(encoded_header_set, &header_list));
  std::map<string, string> header_set(header_list.begin(), header_list.end());
  EXPECT_EQ(expected_header_set, header_set);
}

// TODO(akalin): Add test to exercise emission of the reference set
// once we can decode opcodes that add to the reference set.

}  // namespace

}  // namespace net
