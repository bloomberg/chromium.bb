// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/hpack_decoder.h"

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/strings/string_piece.h"
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

// Utility function to decode a string into a header set, assuming
// that the emitted headers have unique names.
std::map<string, string> DecodeUniqueHeaderSet(
    HpackDecoder* decoder, StringPiece str) {
  HpackHeaderPairVector header_list;
  EXPECT_TRUE(decoder->DecodeHeaderSet(str, &header_list));
  std::map<string, string> header_set(
      header_list.begin(), header_list.end());
  // Make sure |header_list| has no duplicates.
  EXPECT_EQ(header_set.size(), header_list.size());
  return header_set;
}

// Decoding an indexed header should toggle the index's presence in
// the reference set, making a copy of static table entries if
// necessary. It should also emit the header if toggled on (and only
// as many times as it was toggled on).
TEST(HpackDecoderTest, IndexedHeaderBasic) {
  HpackDecoder decoder(kuint32max);

  // Toggle on static table entry #2 (and make a copy at index #1),
  // then toggle on static table entry #5 (which is now #6 because of
  // the copy of #2).
  std::map<string, string> header_set1 =
      DecodeUniqueHeaderSet(&decoder, "\x82\x86");
  std::map<string, string> expected_header_set1;
  expected_header_set1[":method"] = "GET";
  expected_header_set1[":path"] = "/index.html";
  EXPECT_EQ(expected_header_set1, header_set1);

  std::map<string, string> expected_header_set2;
  expected_header_set2[":path"] = "/index.html";
  // Toggle off the copy of static table entry #5.
  std::map<string, string> header_set2 =
      DecodeUniqueHeaderSet(&decoder, "\x82");
  EXPECT_EQ(expected_header_set2, header_set2);
}

// Decoding an indexed header with index 0 should clear the reference
// set.
TEST(HpackDecoderTest, IndexedHeaderZero) {
  HpackDecoder decoder(kuint32max);

  // Toggle on a couple of headers.
  std::map<string, string> header_set1 =
      DecodeUniqueHeaderSet(&decoder, "\x82\x86");
  std::map<string, string> expected_header_set1;
  expected_header_set1[":method"] = "GET";
  expected_header_set1[":path"] = "/index.html";
  EXPECT_EQ(expected_header_set1, header_set1);

  // Toggle index 0 to clear the reference set.
  std::map<string, string> header_set2 =
      DecodeUniqueHeaderSet(&decoder, "\x80");
  std::map<string, string> expected_header_set2;
  EXPECT_EQ(expected_header_set2, header_set2);
}

// Decoding two valid encoded literal headers with no indexing should
// work.
TEST(HpackDecoderTest, LiteralHeaderNoIndexing) {
  HpackDecoder decoder(kuint32max);
  HpackHeaderPairVector header_list;
  // First header with indexed name, second header with string literal
  // name.
  std::map<string, string> header_set =
      DecodeUniqueHeaderSet(
          &decoder, "\x44\x0c/sample/path\x40\x06:path2\x0e/sample/path/2");

  std::map<string, string> expected_header_set;
  expected_header_set[":path"] = "/sample/path";
  expected_header_set[":path2"] = "/sample/path/2";
  EXPECT_EQ(expected_header_set, header_set);
}

// Decoding two valid encoded literal headers with incremental
// indexing and string literal names should work and add the headers
// to the reference set.
TEST(HpackDecoderTest, LiteralHeaderIncrementalIndexing) {
  HpackDecoder decoder(kuint32max);
  std::map<string, string> header_set = DecodeUniqueHeaderSet(
      &decoder,
      StringPiece("\x04\x0c/sample/path\x00\x06:path2\x0e/sample/path/2", 37));

  std::map<string, string> expected_header_set;
  expected_header_set[":path"] = "/sample/path";
  expected_header_set[":path2"] = "/sample/path/2";
  EXPECT_EQ(expected_header_set, header_set);

  // Decoding an empty string should just return the reference set.
  std::map<string, string> header_set2 = DecodeUniqueHeaderSet(&decoder, "");
  EXPECT_EQ(expected_header_set, header_set2);
}

// Decoding literal headers with invalid indices should fail
// gracefully.
TEST(HpackDecoderTest, LiteralHeaderInvalidIndices) {
  HpackDecoder decoder(kuint32max);

  HpackHeaderPairVector header_list;

  // No indexing.

  // One more than the number of static table entries.
  EXPECT_FALSE(decoder.DecodeHeaderSet(StringPiece("\x7d", 1), &header_list));
  EXPECT_FALSE(decoder.DecodeHeaderSet(StringPiece("\x40", 1), &header_list));

  // Incremental indexing.

  // One more than the number of static table entries.
  EXPECT_FALSE(decoder.DecodeHeaderSet(StringPiece("\x3d", 1), &header_list));
  EXPECT_FALSE(decoder.DecodeHeaderSet(StringPiece("\x00", 1), &header_list));
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

}  // namespace

}  // namespace net
