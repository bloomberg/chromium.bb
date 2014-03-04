// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/hpack_decoder.h"

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "net/spdy/hpack_encoder.h"
#include "net/spdy/hpack_input_stream.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

using base::StringPiece;
using std::string;

using testing::ElementsAre;
using testing::Pair;

const size_t kLiteralBound = 1024;

class HpackDecoderTest : public ::testing::Test {
 protected:
  HpackDecoderTest()
      : decoder_(huffman_table_, kLiteralBound) {}

  virtual void SetUp() {
  }

  // Utility function to decode a string into a header set, assuming
  // that the emitted headers have unique names.
  std::map<string, string> DecodeUniqueHeaderSet(StringPiece str) {
    HpackHeaderPairVector header_list;
    EXPECT_TRUE(decoder_.DecodeHeaderSet(str, &header_list));
    std::map<string, string> header_set(
        header_list.begin(), header_list.end());
    // Make sure |header_list| has no duplicates.
    EXPECT_EQ(header_set.size(), header_list.size());
    return header_set;
  }

  HpackHuffmanTable huffman_table_;
  HpackDecoder decoder_;
};

// Decoding an encoded name with a valid string literal should work.
TEST_F(HpackDecoderTest, DecodeNextNameLiteral) {
  HpackInputStream input_stream(kLiteralBound, StringPiece("\x00\x04name", 6));

  StringPiece string_piece;
  EXPECT_TRUE(decoder_.DecodeNextNameForTest(&input_stream, &string_piece));
  EXPECT_EQ("name", string_piece);
  EXPECT_FALSE(input_stream.HasMoreData());
}

TEST_F(HpackDecoderTest, DecodeNextNameLiteralWithHuffmanEncoding) {
  {
    std::vector<HpackHuffmanSymbol> code = HpackRequestHuffmanCode();
    EXPECT_TRUE(huffman_table_.Initialize(&code[0], code.size()));
  }
  char input[] = "\x00\x88\x4e\xb0\x8b\x74\x97\x90\xfa\x7f";
  StringPiece foo(input, arraysize(input) - 1);
  HpackInputStream input_stream(kLiteralBound, foo);

  StringPiece string_piece;
  EXPECT_TRUE(decoder_.DecodeNextNameForTest(&input_stream, &string_piece));
  EXPECT_EQ("custom-key", string_piece);
  EXPECT_FALSE(input_stream.HasMoreData());
}

// Decoding an encoded name with a valid index should work.
TEST_F(HpackDecoderTest, DecodeNextNameIndexed) {
  HpackInputStream input_stream(kLiteralBound, "\x01");

  StringPiece string_piece;
  EXPECT_TRUE(decoder_.DecodeNextNameForTest(&input_stream, &string_piece));
  EXPECT_EQ(":authority", string_piece);
  EXPECT_FALSE(input_stream.HasMoreData());
}

// Decoding an encoded name with an invalid index should fail.
TEST_F(HpackDecoderTest, DecodeNextNameInvalidIndex) {
  // One more than the number of static table entries.
  HpackInputStream input_stream(kLiteralBound, "\x3d");

  StringPiece string_piece;
  EXPECT_FALSE(decoder_.DecodeNextNameForTest(&input_stream, &string_piece));
}

// Decoding an indexed header should toggle the index's presence in
// the reference set, making a copy of static table entries if
// necessary. It should also emit the header if toggled on (and only
// as many times as it was toggled on).
TEST_F(HpackDecoderTest, IndexedHeaderBasic) {
  // Toggle on static table entry #2 (and make a copy at index #1),
  // then toggle on static table entry #5 (which is now #6 because of
  // the copy of #2).
  std::map<string, string> header_set1 =
      DecodeUniqueHeaderSet("\x82\x86");
  std::map<string, string> expected_header_set1;
  expected_header_set1[":method"] = "GET";
  expected_header_set1[":path"] = "/index.html";
  EXPECT_EQ(expected_header_set1, header_set1);

  std::map<string, string> expected_header_set2;
  expected_header_set2[":path"] = "/index.html";
  // Toggle off the copy of static table entry #5.
  std::map<string, string> header_set2 =
      DecodeUniqueHeaderSet("\x82");
  EXPECT_EQ(expected_header_set2, header_set2);
}

// Decoding an indexed header with index 0 should clear the reference
// set.
TEST_F(HpackDecoderTest, IndexedHeaderZero) {
  // Toggle on a couple of headers.
  std::map<string, string> header_set1 =
      DecodeUniqueHeaderSet("\x82\x86");
  std::map<string, string> expected_header_set1;
  expected_header_set1[":method"] = "GET";
  expected_header_set1[":path"] = "/index.html";
  EXPECT_EQ(expected_header_set1, header_set1);

  // Toggle index 0 to clear the reference set.
  std::map<string, string> header_set2 =
      DecodeUniqueHeaderSet("\x80");
  std::map<string, string> expected_header_set2;
  EXPECT_EQ(expected_header_set2, header_set2);
}

// Decoding two valid encoded literal headers with no indexing should
// work.
TEST_F(HpackDecoderTest, LiteralHeaderNoIndexing) {
  HpackHeaderPairVector header_list;
  // First header with indexed name, second header with string literal
  // name.
  std::map<string, string> header_set =
      DecodeUniqueHeaderSet(
          "\x44\x0c/sample/path\x40\x06:path2\x0e/sample/path/2");

  std::map<string, string> expected_header_set;
  expected_header_set[":path"] = "/sample/path";
  expected_header_set[":path2"] = "/sample/path/2";
  EXPECT_EQ(expected_header_set, header_set);
}

// Decoding two valid encoded literal headers with incremental
// indexing and string literal names should work and add the headers
// to the reference set.
TEST_F(HpackDecoderTest, LiteralHeaderIncrementalIndexing) {
  std::map<string, string> header_set = DecodeUniqueHeaderSet(
      StringPiece("\x04\x0c/sample/path\x00\x06:path2\x0e/sample/path/2", 37));

  std::map<string, string> expected_header_set;
  expected_header_set[":path"] = "/sample/path";
  expected_header_set[":path2"] = "/sample/path/2";
  EXPECT_EQ(expected_header_set, header_set);

  // Decoding an empty string should just return the reference set.
  std::map<string, string> header_set2 = DecodeUniqueHeaderSet("");
  EXPECT_EQ(expected_header_set, header_set2);
}

// Decoding literal headers with invalid indices should fail
// gracefully.
TEST_F(HpackDecoderTest, LiteralHeaderInvalidIndices) {
  HpackHeaderPairVector header_list;

  // No indexing.

  // One more than the number of static table entries.
  EXPECT_FALSE(decoder_.DecodeHeaderSet(StringPiece("\x7d", 1), &header_list));
  EXPECT_FALSE(decoder_.DecodeHeaderSet(StringPiece("\x40", 1), &header_list));

  // Incremental indexing.

  // One more than the number of static table entries.
  EXPECT_FALSE(decoder_.DecodeHeaderSet(StringPiece("\x3d", 1), &header_list));
  EXPECT_FALSE(decoder_.DecodeHeaderSet(StringPiece("\x00", 1), &header_list));
}

// Round-tripping the header set from E.2.1 should work.
TEST_F(HpackDecoderTest, BasicE21) {
  HpackEncoder encoder(kLiteralBound);

  std::map<string, string> expected_header_set;
  expected_header_set[":method"] = "GET";
  expected_header_set[":scheme"] = "http";
  expected_header_set[":path"] = "/";
  expected_header_set[":authority"] = "www.example.com";

  string encoded_header_set;
  EXPECT_TRUE(encoder.EncodeHeaderSet(
      expected_header_set, &encoded_header_set));

  HpackHeaderPairVector header_list;
  EXPECT_TRUE(decoder_.DecodeHeaderSet(encoded_header_set, &header_list));
  std::map<string, string> header_set(header_list.begin(), header_list.end());
  EXPECT_EQ(expected_header_set, header_set);
}

TEST_F(HpackDecoderTest, SectionD3RequestHuffmanExamples) {
  {
    std::vector<HpackHuffmanSymbol> code = HpackRequestHuffmanCode();
    EXPECT_TRUE(huffman_table_.Initialize(&code[0], code.size()));
  }
  std::map<string, string> header_set;

  // 82                                      | == Indexed - Add ==
  //                                         |   idx = 2
  //                                         | -> :method: GET
  // 87                                      | == Indexed - Add ==
  //                                         |   idx = 7
  //                                         | -> :scheme: http
  // 86                                      | == Indexed - Add ==
  //                                         |   idx = 6
  //                                         | -> :path: /
  // 04                                      | == Literal indexed ==
  //                                         |   Indexed name (idx = 4)
  //                                         |     :authority
  // 8b                                      |   Literal value (len = 15)
  //                                         |     Huffman encoded:
  // db6d 883e 68d1 cb12 25ba 7f             | .m..h...%..
  //                                         |     Decoded:
  //                                         | www.example.com
  //                                         | -> :authority: www.example.com
  char first[] =
      "\x82\x87\x86\x04\x8b\xdb\x6d\x88\x3e\x68\xd1\xcb\x12\x25\xba\x7f";
  header_set = DecodeUniqueHeaderSet(StringPiece(first, arraysize(first)-1));

  // TODO(jgraettinger): Create HpackEncodingContext and
  // HpackDecoder peers, and inspect the header table here.
  EXPECT_THAT(header_set, ElementsAre(
      Pair(":authority", "www.example.com"),
      Pair(":method", "GET"),
      Pair(":path", "/"),
      Pair(":scheme", "http")));

  // 1b                                      | == Literal indexed ==
  //                                         |   Indexed name (idx = 27)
  //                                         |     cache-control
  // 86                                      |   Literal value (len = 8)
  //                                         |     Huffman encoded:
  // 6365 4a13 98ff                          | ceJ...
  //                                         |     Decoded:
  //                                         | no-cache
  //                                         | -> cache-control: no-cache
  char second[] = "\x1b\x86\x63\x65\x4a\x13\x98\xff";
  header_set = DecodeUniqueHeaderSet(StringPiece(second, arraysize(second)-1));

  EXPECT_THAT(header_set, ElementsAre(
      Pair(":authority", "www.example.com"),
      Pair(":method", "GET"),
      Pair(":path", "/"),
      Pair(":scheme", "http"),
      Pair("cache-control", "no-cache")));

  // 80                                      | == Empty reference set ==
  //                                         |   idx = 0
  //                                         |   flag = 1
  // 85                                      | == Indexed - Add ==
  //                                         |   idx = 5
  //                                         | -> :method: GET
  // 8c                                      | == Indexed - Add ==
  //                                         |   idx = 12
  //                                         | -> :scheme: https
  // 8b                                      | == Indexed - Add ==
  //                                         |   idx = 11
  //                                         | -> :path: /index.html
  // 84                                      | == Indexed - Add ==
  //                                         |   idx = 4
  //                                         | -> :authority: www.example.com
  // 00                                      | == Literal indexed ==
  // 88                                      |   Literal name (len = 10)
  //                                         |     Huffman encoded:
  // 4eb0 8b74 9790 fa7f                     | N..t....
  //                                         |     Decoded:
  //                                         | custom-key
  // 89                                      |   Literal value (len = 12)
  //                                         |     Huffman encoded:
  // 4eb0 8b74 979a 17a8 ff                  | N..t.....
  //                                         |     Decoded:
  //                                         | custom-value
  //                                         | -> custom-key: custom-value
  char third[] =
      "\x80\x85\x8c\x8b\x84\x00\x88\x4e\xb0\x8b\x74\x97\x90\xfa\x7f\x89"
      "\x4e\xb0\x8b\x74\x97\x9a\x17\xa8\xff";
  header_set = DecodeUniqueHeaderSet(StringPiece(third, arraysize(third)-1));

  EXPECT_THAT(header_set, ElementsAre(
      Pair(":authority", "www.example.com"),
      Pair(":method", "GET"),
      Pair(":path", "/index.html"),
      Pair(":scheme", "https"),
      Pair("custom-key", "custom-value")));
}

TEST_F(HpackDecoderTest, SectionD5ResponseHuffmanExamples) {
  {
    std::vector<HpackHuffmanSymbol> code = HpackResponseHuffmanCode();
    EXPECT_TRUE(huffman_table_.Initialize(&code[0], code.size()));
  }
  std::map<string, string> header_set;
  decoder_.SetMaxHeadersSize(256);

  // 08                                      | == Literal indexed ==
  //                                         |   Indexed name (idx = 8)
  //                                         |     :status
  // 82                                      |   Literal value (len = 3)
  //                                         |     Huffman encoded:
  // 409f                                    | @.
  //                                         |     Decoded:
  //                                         | 302
  //                                         | -> :status: 302
  // 18                                      | == Literal indexed ==
  //                                         |   Indexed name (idx = 24)
  //                                         |     cache-control
  // 86                                      |   Literal value (len = 7)
  //                                         |     Huffman encoded:
  // c31b 39bf 387f                          | ..9.8.
  //                                         |     Decoded:
  //                                         | private
  //                                         | -> cache-control: private
  // 22                                      | == Literal indexed ==
  //                                         |   Indexed name (idx = 34)
  //                                         |     date
  // 92                                      |   Literal value (len = 29)
  //                                         |     Huffman encoded:
  // a2fb a203 20f2 ab30 3124 018b 490d 3209 | .... ..01$..I.2.
  // e877                                    | .w
  //                                         |     Decoded:
  //                                         | Mon, 21 Oct 2013 20:13:21
  //                                         | GMT
  //                                         | -> date: Mon, 21 Oct 2013
  //                                         |   20:13:21 GMT
  // 30                                      | == Literal indexed ==
  //                                         |   Indexed name (idx = 48)
  //                                         |     location
  // 93                                      |   Literal value (len = 23)
  //                                         |     Huffman encoded:
  // e39e 7864 dd7a fd3d 3d24 8747 db87 2849 | ..xd.z.==$.G..(I
  // 55f6 ff                                 | U..
  //                                         |     Decoded:
  //                                         | https://www.example.com
  //                                         | -> location: https://www.e
  //                                         |    xample.com
  char first[] =
      "\x08\x82\x40\x9f\x18\x86\xc3\x1b\x39\xbf\x38\x7f\x22\x92\xa2\xfb"
      "\xa2\x03\x20\xf2\xab\x30\x31\x24\x01\x8b\x49\x0d\x32\x09\xe8\x77"
      "\x30\x93\xe3\x9e\x78\x64\xdd\x7a\xfd\x3d\x3d\x24\x87\x47\xdb\x87"
      "\x28\x49\x55\xf6\xff";
  header_set = DecodeUniqueHeaderSet(StringPiece(first, arraysize(first)-1));

  EXPECT_THAT(header_set, ElementsAre(
      Pair(":status", "302"),
      Pair("cache-control", "private"),
      Pair("date", "Mon, 21 Oct 2013 20:13:21 GMT"),
      Pair("location", "https://www.example.com")));

  // 84                                      | == Indexed - Remove ==
  //                                         |   idx = 4
  //                                         | -> :status: 302
  // 8c                                      | == Indexed - Add ==
  //                                         |   idx = 12
  //                                         | - evict: :status: 302
  //                                         | -> :status: 200
  char second[] = "\x84\x8c";
  header_set = DecodeUniqueHeaderSet(StringPiece(second, arraysize(second)-1));

  EXPECT_THAT(header_set, ElementsAre(
      Pair(":status", "200"),
      Pair("cache-control", "private"),
      Pair("date", "Mon, 21 Oct 2013 20:13:21 GMT"),
      Pair("location", "https://www.example.com")));

  // 83                                      | == Indexed - Remove ==
  //                                         |   idx = 3
  //                                         | -> date: Mon, 21 Oct 2013
  //                                         |   20:13:21 GMT
  // 84                                      | == Indexed - Remove ==
  //                                         |   idx = 4
  //                                         | -> cache-control: private
  // 84                                      | == Indexed - Add ==
  //                                         |   idx = 4
  //                                         | -> cache-control: private
  // 03                                      | == Literal indexed ==
  //                                         |   Indexed name (idx = 3)
  //                                         |     date
  // 92                                      |   Literal value (len = 29)
  //                                         |     Huffman encoded:
  // a2fb a203 20f2 ab30 3124 018b 490d 3309 | .... ..01$..I.3.
  // e877                                    | .w
  //                                         |     Decoded:
  //                                         | Mon, 21 Oct 2013 20:13:22 GMT
  //                                         | - evict: cache-control: private
  //                                         | -> date: Mon, 21 Oct 2013
  //                                         |   20:13:22 GMT
  // 1d                                      | == Literal indexed ==
  //                                         |   Indexed name (idx = 29)
  //                                         |     content-encoding
  // 84                                      |   Literal value (len = 4)
  //                                         |     Huffman encoded:
  // e1fb b30f                               | ....
  //                                         |     Decoded:
  //                                         | gzip
  //                                         | - evict: date: Mon, 21 Oct
  //                                         |    2013 20:13:21 GMT
  //                                         | -> content-encoding: gzip
  // 84                                      | == Indexed - Remove ==
  //                                         |   idx = 4
  //                                         | -> location: https://www.e
  //                                         |    xample.com
  // 84                                      | == Indexed - Add ==
  //                                         |   idx = 4
  //                                         | -> location: https://www.e
  //                                         |    xample.com
  // 83                                      | == Indexed - Remove ==
  //                                         |   idx = 3
  //                                         | -> :status: 200
  // 83                                      | == Indexed - Add ==
  //                                         |   idx = 3
  //                                         | -> :status: 200
  // 3a                                      | == Literal indexed ==
  //                                         |   Indexed name (idx = 58)
  //                                         |     set-cookie
  // b3                                      |   Literal value (len = 56)
  //                                         |     Huffman encoded:
  // df7d fb36 d3d9 e1fc fc3f afe7 abfc fefc | .}.6.....?......
  // bfaf 3edf 2f97 7fd3 6ff7 fd79 f6f9 77fd | ..../...o..y..w.
  // 3de1 6bfa 46fe 10d8 8944 7de1 ce18 e565 | =.k.F....D}....e
  // f76c 2f                                 | .l/
  //                                         |     Decoded:
  //                                         | foo=ASDJKHQKBZXOQWEOPIUAXQ
  //                                         | WEOIU; max-age=3600; versi
  //                                         | on=1
  //                                         | - evict: location: https:/
  //                                         |    /www.example.com
  //                                         | - evict: :status: 200
  //                                         | -> set-cookie: foo=ASDJKHQ
  //                                         |   KBZXOQWEOPIUAXQWEOIU; ma
  //                                         |   x-age=3600; version=1
  char third[] =
      "\x83\x84\x84\x03\x92\xa2\xfb\xa2\x03\x20\xf2\xab\x30\x31\x24\x01"
      "\x8b\x49\x0d\x33\x09\xe8\x77\x1d\x84\xe1\xfb\xb3\x0f\x84\x84\x83"
      "\x83\x3a\xb3\xdf\x7d\xfb\x36\xd3\xd9\xe1\xfc\xfc\x3f\xaf\xe7\xab"
      "\xfc\xfe\xfc\xbf\xaf\x3e\xdf\x2f\x97\x7f\xd3\x6f\xf7\xfd\x79\xf6"
      "\xf9\x77\xfd\x3d\xe1\x6b\xfa\x46\xfe\x10\xd8\x89\x44\x7d\xe1\xce"
      "\x18\xe5\x65\xf7\x6c\x2f";
  header_set = DecodeUniqueHeaderSet(StringPiece(third, arraysize(third)-1));

  EXPECT_THAT(header_set, ElementsAre(
      Pair(":status", "200"),
      Pair("cache-control", "private"),
      Pair("content-encoding", "gzip"),
      Pair("date", "Mon, 21 Oct 2013 20:13:22 GMT"),
      Pair("location", "https://www.example.com"),
      Pair("set-cookie", "foo=ASDJKHQKBZXOQWEOPIUAXQWEOIU;"
           " max-age=3600; version=1")));
}

}  // namespace

}  // namespace net
