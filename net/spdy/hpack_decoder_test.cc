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
#include "net/spdy/hpack_output_stream.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace test {

using base::StringPiece;
using std::string;

class HpackEncodingContextPeer {
 public:
  explicit HpackEncodingContextPeer(const HpackEncodingContext& context)
      : context_(context) {}
  const HpackHeaderTable& header_table() {
    return context_.header_table_;
  }

 private:
  const HpackEncodingContext& context_;
};

class HpackDecoderPeer {
 public:
  explicit HpackDecoderPeer(HpackDecoder* decoder)
      : decoder_(decoder) {}

  void HandleHeaderRepresentation(StringPiece name, StringPiece value) {
    decoder_->HandleHeaderRepresentation(name, value);
  }
  bool DecodeNextName(HpackInputStream* in, StringPiece* out) {
    return decoder_->DecodeNextName(in, out);
  }
  HpackEncodingContextPeer context_peer() {
    return HpackEncodingContextPeer(decoder_->context_);
  }
  void set_cookie_name(string name) {
    decoder_->cookie_name_ = name;
  }
  string cookie_name() {
    return decoder_->cookie_name_;
  }
  void set_cookie_value(string value) {
    decoder_->cookie_value_ = value;
  }
  string cookie_value() {
    return decoder_->cookie_value_;
  }
  const std::map<string, string>& decoded_block() const {
    return decoder_->decoded_block_;
  }
  const string& headers_block_buffer() const {
    return decoder_->headers_block_buffer_;
  }

 private:
  HpackDecoder* decoder_;
};

}  // namespace test

namespace {

using base::StringPiece;
using std::string;

using testing::ElementsAre;
using testing::Pair;

const size_t kLiteralBound = 1024;

class HpackDecoderTest : public ::testing::Test {
 protected:
  HpackDecoderTest()
      : decoder_(ObtainHpackHuffmanTable()),
        decoder_peer_(&decoder_),
        context_peer_(decoder_peer_.context_peer()) {}

  bool DecodeHeaderBlock(StringPiece str) {
    return decoder_.HandleControlFrameHeadersData(0, str.data(), str.size()) &&
        decoder_.HandleControlFrameHeadersComplete(0);
  }
  const std::map<string, string>& decoded_block() const {
    // TODO(jgraettinger): HpackDecoderTest should implement
    // SpdyHeadersHandlerInterface, and collect headers for examination.
    return decoder_peer_.decoded_block();
  }
  // TODO(jgraettinger): Eliminate uses of this in tests below. Prefer
  // DecodeHeaderBlock().
  const std::map<string, string>& DecodeUniqueHeaderSet(StringPiece str) {
    EXPECT_TRUE(DecodeHeaderBlock(str));
    return decoded_block();
  }

  HpackDecoder decoder_;
  test::HpackDecoderPeer decoder_peer_;
  test::HpackEncodingContextPeer context_peer_;
};

TEST_F(HpackDecoderTest, HandleControlFrameHeadersData) {
  // Strings under threshold are concatenated in the buffer.
  EXPECT_TRUE(decoder_.HandleControlFrameHeadersData(
      0, "small string one", 16));
  EXPECT_TRUE(decoder_.HandleControlFrameHeadersData(
      0, "small string two", 16));
  // A string which would push the buffer over the threshold is refused.
  EXPECT_FALSE(decoder_.HandleControlFrameHeadersData(
      0, "fails", kMaxDecodeBufferSize - 32 + 1));

  EXPECT_EQ(decoder_peer_.headers_block_buffer(),
            "small string onesmall string two");
}

TEST_F(HpackDecoderTest, HandleControlFrameHeadersComplete) {
  // Decode a block which toggles two static headers into the reference set.
  EXPECT_TRUE(DecodeHeaderBlock("\x82\x86"));

  decoder_peer_.set_cookie_name("CooKie");
  decoder_peer_.set_cookie_value("foobar=baz");

  // Headers in the reference set should be emitted.
  // Incremental cookie buffer should be emitted and cleared.
  decoder_.HandleControlFrameHeadersData(0, NULL, 0);
  decoder_.HandleControlFrameHeadersComplete(0);

  EXPECT_THAT(decoded_block(), ElementsAre(
      Pair(":method", "GET"),
      Pair(":path", "/index.html"),
      Pair("CooKie", "foobar=baz")));

  EXPECT_EQ(decoder_peer_.cookie_name(), "");
  EXPECT_EQ(decoder_peer_.cookie_value(), "");
}

TEST_F(HpackDecoderTest, HandleHeaderRepresentation) {
  // Casing of first Cookie is retained, but all instances are joined.
  decoder_peer_.HandleHeaderRepresentation("cOOkie", " part 1");
  decoder_peer_.HandleHeaderRepresentation("cookie", "part 2 ");
  decoder_peer_.HandleHeaderRepresentation("cookie", "part3");

  // Already-delimited headers are passed through.
  decoder_peer_.HandleHeaderRepresentation("passed-through",
                                           string("foo\0baz", 7));

  // Other headers are joined on \0. Case matters.
  decoder_peer_.HandleHeaderRepresentation("joined", "not joined");
  decoder_peer_.HandleHeaderRepresentation("joineD", "value 1");
  decoder_peer_.HandleHeaderRepresentation("joineD", "value 2");

  // Empty headers remain empty.
  decoder_peer_.HandleHeaderRepresentation("empty", "");

  // Joined empty headers work as expected.
  decoder_peer_.HandleHeaderRepresentation("empty-joined", "");
  decoder_peer_.HandleHeaderRepresentation("empty-joined", "foo");
  decoder_peer_.HandleHeaderRepresentation("empty-joined", "");
  decoder_peer_.HandleHeaderRepresentation("empty-joined", "");

  // Non-contiguous cookie crumb.
  decoder_peer_.HandleHeaderRepresentation("Cookie", " fin!");

  // Finish and emit all headers.
  decoder_.HandleControlFrameHeadersComplete(0);

  EXPECT_THAT(decoded_block(), ElementsAre(
      Pair("cOOkie", " part 1; part 2 ; part3;  fin!"),
      Pair("empty", ""),
      Pair("empty-joined", string("\0foo\0\0", 6)),
      Pair("joineD", string("value 1\0value 2", 15)),
      Pair("joined", "not joined"),
      Pair("passed-through", string("foo\0baz", 7))));
}

// Decoding an encoded name with a valid string literal should work.
TEST_F(HpackDecoderTest, DecodeNextNameLiteral) {
  HpackInputStream input_stream(kLiteralBound, StringPiece("\x00\x04name", 6));

  StringPiece string_piece;
  EXPECT_TRUE(decoder_peer_.DecodeNextName(&input_stream, &string_piece));
  EXPECT_EQ("name", string_piece);
  EXPECT_FALSE(input_stream.HasMoreData());
}

TEST_F(HpackDecoderTest, DecodeNextNameLiteralWithHuffmanEncoding) {
  char input[] = "\x00\x88\x4e\xb0\x8b\x74\x97\x90\xfa\x7f";
  StringPiece foo(input, arraysize(input) - 1);
  HpackInputStream input_stream(kLiteralBound, foo);

  StringPiece string_piece;
  EXPECT_TRUE(decoder_peer_.DecodeNextName(&input_stream, &string_piece));
  EXPECT_EQ("custom-key", string_piece);
  EXPECT_FALSE(input_stream.HasMoreData());
}

// Decoding an encoded name with a valid index should work.
TEST_F(HpackDecoderTest, DecodeNextNameIndexed) {
  HpackInputStream input_stream(kLiteralBound, "\x01");

  StringPiece string_piece;
  EXPECT_TRUE(decoder_peer_.DecodeNextName(&input_stream, &string_piece));
  EXPECT_EQ(":authority", string_piece);
  EXPECT_FALSE(input_stream.HasMoreData());
}

// Decoding an encoded name with an invalid index should fail.
TEST_F(HpackDecoderTest, DecodeNextNameInvalidIndex) {
  // One more than the number of static table entries.
  HpackInputStream input_stream(kLiteralBound, "\x3d");

  StringPiece string_piece;
  EXPECT_FALSE(decoder_peer_.DecodeNextName(&input_stream, &string_piece));
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

// Test a too-large indexed header.
TEST_F(HpackDecoderTest, InvalidIndexedHeader) {
  // High-bit set, and a prefix of one more than the number of static entries.
  EXPECT_FALSE(DecodeHeaderBlock(StringPiece("\xbd", 1)));
}

TEST_F(HpackDecoderTest, ContextUpdateMaximumSize) {
  EXPECT_EQ(kDefaultHeaderTableSizeSetting,
            context_peer_.header_table().max_size());
  {
    // Maximum-size update with size 126. Succeeds.
    EXPECT_TRUE(DecodeHeaderBlock(StringPiece("\x80\x7e", 2)));
    EXPECT_EQ(126u, context_peer_.header_table().max_size());
  }
  string input;
  {
    // Maximum-size update with kDefaultHeaderTableSizeSetting. Succeeds.
    HpackOutputStream output_stream(kuint32max);
    output_stream.AppendBits(0x80, 8);  // Context update.
    output_stream.AppendBits(0x00, 1);  // Size update.
    output_stream.AppendUint32ForTest(kDefaultHeaderTableSizeSetting);

    output_stream.TakeString(&input);
    EXPECT_TRUE(DecodeHeaderBlock(StringPiece(input)));
    EXPECT_EQ(kDefaultHeaderTableSizeSetting,
              context_peer_.header_table().max_size());
  }
  {
    // Maximum-size update with kDefaultHeaderTableSizeSetting + 1. Fails.
    HpackOutputStream output_stream(kuint32max);
    output_stream.AppendBits(0x80, 8);  // Context update.
    output_stream.AppendBits(0x00, 1);  // Size update.
    output_stream.AppendUint32ForTest(kDefaultHeaderTableSizeSetting + 1);

    output_stream.TakeString(&input);
    EXPECT_FALSE(DecodeHeaderBlock(StringPiece(input)));
    EXPECT_EQ(kDefaultHeaderTableSizeSetting,
              context_peer_.header_table().max_size());
  }
}

TEST_F(HpackDecoderTest, ContextUpdateClearReferenceSet) {
  // Toggle on a couple of headers.
  std::map<string, string> header_set1 =
      DecodeUniqueHeaderSet("\x82\x86");
  std::map<string, string> expected_header_set1;
  expected_header_set1[":method"] = "GET";
  expected_header_set1[":path"] = "/index.html";
  EXPECT_EQ(expected_header_set1, header_set1);

  // Send a context update to clear the reference set.
  std::map<string, string> header_set2 =
      DecodeUniqueHeaderSet("\x80\x80");
  std::map<string, string> expected_header_set2;
  EXPECT_EQ(expected_header_set2, header_set2);
}

// Decoding two valid encoded literal headers with no indexing should
// work.
TEST_F(HpackDecoderTest, LiteralHeaderNoIndexing) {
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
  // No indexing.

  // One more than the number of static table entries.
  EXPECT_FALSE(DecodeHeaderBlock(StringPiece("\x7d", 1)));
  EXPECT_FALSE(DecodeHeaderBlock(StringPiece("\x40", 1)));

  // Incremental indexing.

  // One more than the number of static table entries.
  EXPECT_FALSE(DecodeHeaderBlock(StringPiece("\x3d", 1)));
  EXPECT_FALSE(DecodeHeaderBlock(StringPiece("\x00", 1)));
}

// Round-tripping the header set from E.2.1 should work.
TEST_F(HpackDecoderTest, BasicE21) {
  HpackEncoder encoder;

  std::map<string, string> expected_header_set;
  expected_header_set[":method"] = "GET";
  expected_header_set[":scheme"] = "http";
  expected_header_set[":path"] = "/";
  expected_header_set[":authority"] = "www.example.com";

  string encoded_header_set;
  EXPECT_TRUE(encoder.EncodeHeaderSet(
      expected_header_set, &encoded_header_set));

  EXPECT_TRUE(DecodeHeaderBlock(encoded_header_set));
  EXPECT_EQ(expected_header_set, decoded_block());
}

TEST_F(HpackDecoderTest, SectionD3RequestHuffmanExamples) {
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

  // 8080                                    | == Empty reference set ==
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
      "\x80\x80\x85\x8c\x8b\x84\x00\x88\x4e\xb0\x8b\x74\x97\x90\xfa\x7f\x89"
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
  std::map<string, string> header_set;
  decoder_.ApplyHeaderTableSizeSetting(256);

  // 08                                      | == Literal indexed ==
  //                                         |   Indexed name (idx = 8)
  //                                         |     :status
  // 82                                      |   Literal value (len = 3)
  //                                         |     Huffman encoded:
  // 98a7                                    | ..
  //                                         |     Decoded:
  //                                         | 302
  //                                         | -> :status: 302
  // 18                                      | == Literal indexed ==
  //                                         |   Indexed name (idx = 24)
  //                                         |     cache-control
  // 85                                      |   Literal value (len = 7)
  //                                         |     Huffman encoded:
  // 73d5 cd11 1f                            | s....
  //                                         |     Decoded:
  //                                         | private
  //                                         | -> cache-control: private
  // 22                                      | == Literal indexed ==
  //                                         |   Indexed name (idx = 34)
  //                                         |     date
  // 98                                      |   Literal value (len = 29)
  //                                         |     Huffman encoded:
  // ef6b 3a7a 0e6e 8fa2 63d0 729a 6e83 97d8 | .k:z.n..c.r.n...
  // 69bd 8737 47bb bfc7                     | i..7G...
  //                                         |     Decoded:
  //                                         | Mon, 21 Oct 2013 20:13:21
  //                                         | GMT
  //                                         | -> date: Mon, 21 Oct 2013
  //                                         |   20:13:21 GMT
  // 30                                      | == Literal indexed ==
  //                                         |   Indexed name (idx = 48)
  //                                         |     location
  // 90                                      |   Literal value (len = 23)
  //                                         |     Huffman encoded:
  // ce31 743d 801b 6db1 07cd 1a39 6244 b74f | .1t=..m....9bD.O
  //                                         |     Decoded:
  //                                         | https://www.example.com
  //                                         | -> location: https://www.e
  //                                         |    xample.com
  char first[] =
      "\x08\x82\x98\xa7\x18\x85\x73\xd5\xcd\x11\x1f\x22\x98\xef\x6b"
      "\x3a\x7a\x0e\x6e\x8f\xa2\x63\xd0\x72\x9a\x6e\x83\x97\xd8\x69\xbd\x87"
      "\x37\x47\xbb\xbf\xc7\x30\x90\xce\x31\x74\x3d\x80\x1b\x6d\xb1\x07\xcd"
      "\x1a\x39\x62\x44\xb7\x4f";
  header_set = DecodeUniqueHeaderSet(StringPiece(first, arraysize(first)-1));

  EXPECT_THAT(header_set, ElementsAre(
      Pair(":status", "302"),
      Pair("cache-control", "private"),
      Pair("date", "Mon, 21 Oct 2013 20:13:21 GMT"),
      Pair("location", "https://www.example.com")));

  // 8c                                      | == Indexed - Add ==
  //                                         |   idx = 12
  //                                         | - evict: :status: 302
  //                                         | -> :status: 200
  char second[] = "\x8c";
  header_set = DecodeUniqueHeaderSet(StringPiece(second, arraysize(second)-1));

  EXPECT_THAT(header_set, ElementsAre(
      Pair(":status", "200"),
      Pair("cache-control", "private"),
      Pair("date", "Mon, 21 Oct 2013 20:13:21 GMT"),
      Pair("location", "https://www.example.com")));

  // 84                                      | == Indexed - Remove ==
  //                                         |   idx = 4
  //                                         | -> cache-control: private
  // 84                                      | == Indexed - Add ==
  //                                         |   idx = 4
  //                                         | -> cache-control: private
  // 03                                      | == Literal indexed ==
  //                                         |   Indexed name (idx = 3)
  //                                         |     date
  // 98                                      |   Literal value (len = 29)
  //                                         |     Huffman encoded:
  // ef6b 3a7a 0e6e 8fa2 63d0 729a 6e83 97d8 | .k:z.n..c.r.n...
  // 69bd 873f 47bb bfc7                     | i..?G...
  //                                         |     Decoded:
  //                                         | Mon, 21 Oct 2013 20:13:22
  //                                         | GMT
  //                                         | - evict: cache-control: pr
  //                                         |   ivate
  //                                         | -> date: Mon, 21 Oct 2013
  //                                         |   20:13:22 GMT
  // 1d                                      | == Literal indexed ==
  //                                         |   Indexed name (idx = 29)
  //                                         |     content-encoding
  // 83                                      |   Literal value (len = 4)
  //                                         |     Huffman encoded:
  // cbd5 4e                                 | ..N
  //                                         |     Decoded:
  //                                         | gzip
  //                                         | - evict: date: Mon, 21 Oct
  //                                         |    2013 20:13:21 GMT
  //                                         | -> content-encoding: gzip
  // 84                                      | == Indexed - Remove ==
  //                                         |   idx = 4
  //                                         | -> location: https://www.e
  //                                         |   xample.com
  // 84                                      | == Indexed - Add ==
  //                                         |   idx = 4
  //                                         | -> location: https://www.e
  //                                         |   xample.com
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
  // c5ad b77f 876f c7fb f7fd bfbe bff3 f7f4 | .....o..........
  // fb7e bbbe 9f5f 87e3 7fef edfa eefa 7c3f | ....._........|?
  // 1d5d 1a23 ce54 6436 cd49 4bd5 d1cc 5f05 | .].#.Td6.IK..._.
  // 3596 9b                                 | 5..
  //                                         |     Decoded:
  //                                         | foo=ASDJKHQKBZXOQWEOPIUAXQ
  //                                         | WEOIU; max-age=3600; versi
  //                                         | on=1
  //                                         | - evict: location: https:/
  //                                         |   /www.example.com
  //                                         | - evict: :status: 200
  //                                         | -> set-cookie: foo=ASDJKHQ
  //                                         |   KBZXOQWEOPIUAXQWEOIU; ma
  //                                         |   x-age=3600; version=1
  char third[] =
      "\x84\x84\x03\x98\xef\x6b\x3a\x7a\x0e\x6e\x8f\xa2\x63\xd0\x72"
      "\x9a\x6e\x83\x97\xd8\x69\xbd\x87\x3f\x47\xbb\xbf\xc7\x1d\x83\xcb\xd5"
      "\x4e\x84\x84\x83\x83\x3a\xb3\xc5\xad\xb7\x7f\x87\x6f\xc7\xfb\xf7\xfd"
      "\xbf\xbe\xbf\xf3\xf7\xf4\xfb\x7e\xbb\xbe\x9f\x5f\x87\xe3\x7f\xef\xed"
      "\xfa\xee\xfa\x7c\x3f\x1d\x5d\x1a\x23\xce\x54\x64\x36\xcd\x49\x4b\xd5"
      "\xd1\xcc\x5f\x05\x35\x96\x9b";
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
