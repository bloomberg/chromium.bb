// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/hpack_encoder.h"

#include <map>
#include <string>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

using base::StringPiece;
using std::string;
using testing::ElementsAre;

namespace test {

class HpackHeaderTablePeer {
 public:
  explicit HpackHeaderTablePeer(HpackHeaderTable* table)
      : table_(table) {}

  HpackHeaderTable::EntryTable* dynamic_entries() {
    return &table_->dynamic_entries_;
  }

 private:
  HpackHeaderTable* table_;
};

class HpackEncoderPeer {
 public:
  typedef HpackEncoder::Representation Representation;
  typedef HpackEncoder::Representations Representations;

  explicit HpackEncoderPeer(HpackEncoder* encoder)
    : encoder_(encoder) {}

  HpackHeaderTable* table() {
    return &encoder_->header_table_;
  }
  HpackHeaderTablePeer table_peer() {
    return HpackHeaderTablePeer(table());
  }
  bool allow_huffman_compression() {
    return encoder_->allow_huffman_compression_;
  }
  void set_allow_huffman_compression(bool allow) {
    encoder_->allow_huffman_compression_ = allow;
  }
  void EmitString(StringPiece str) {
    encoder_->EmitString(str);
  }
  void TakeString(string* out) {
    encoder_->output_stream_.TakeString(out);
  }
  void UpdateCharacterCounts(StringPiece str) {
    encoder_->UpdateCharacterCounts(str);
  }
  static void CookieToCrumbs(StringPiece cookie,
                             std::vector<StringPiece>* out) {
    Representations tmp;
    HpackEncoder::CookieToCrumbs(make_pair("", cookie), &tmp);

    out->clear();
    for (size_t i = 0; i != tmp.size(); ++i) {
      out->push_back(tmp[i].second);
    }
  }
  static void DecomposeRepresentation(StringPiece value,
                                      std::vector<StringPiece>* out) {
    Representations tmp;
    HpackEncoder::DecomposeRepresentation(make_pair("foobar", value), &tmp);

    out->clear();
    for (size_t i = 0; i != tmp.size(); ++i) {
      out->push_back(tmp[i].second);
    }
  }

 private:
  HpackEncoder* encoder_;
};

}  // namespace test

namespace {

using std::map;
using testing::ElementsAre;

class HpackEncoderTest : public ::testing::Test {
 protected:
  typedef test::HpackEncoderPeer::Representations Representations;

  HpackEncoderTest()
      : encoder_(ObtainHpackHuffmanTable()),
        peer_(&encoder_),
        static_(peer_.table()->GetByIndex(1)) {}

  virtual void SetUp() {
    // Populate dynamic entries into the table fixture. For simplicity each
    // entry has name.size() + value.size() == 10.
    key_1_ = peer_.table()->TryAddEntry("key1", "value1");
    key_2_ = peer_.table()->TryAddEntry("key2", "value2");
    cookie_a_ = peer_.table()->TryAddEntry("cookie", "a=bb");
    cookie_c_ = peer_.table()->TryAddEntry("cookie", "c=dd");

    // No further insertions may occur without evictions.
    peer_.table()->SetMaxSize(peer_.table()->size());

    // Disable Huffman coding by default. Most tests don't care about it.
    peer_.set_allow_huffman_compression(false);
  }

  void ExpectIndex(size_t index) {
    expected_.AppendPrefix(kIndexedOpcode);
    expected_.AppendUint32(index);
  }
  void ExpectIndexedLiteral(const HpackEntry* key_entry, StringPiece value) {
    expected_.AppendPrefix(kLiteralIncrementalIndexOpcode);
    expected_.AppendUint32(IndexOf(key_entry));
    expected_.AppendPrefix(kStringLiteralIdentityEncoded);
    expected_.AppendUint32(value.size());
    expected_.AppendBytes(value);
  }
  void ExpectIndexedLiteral(StringPiece name, StringPiece value) {
    expected_.AppendPrefix(kLiteralIncrementalIndexOpcode);
    expected_.AppendUint32(0);
    expected_.AppendPrefix(kStringLiteralIdentityEncoded);
    expected_.AppendUint32(name.size());
    expected_.AppendBytes(name);
    expected_.AppendPrefix(kStringLiteralIdentityEncoded);
    expected_.AppendUint32(value.size());
    expected_.AppendBytes(value);
  }
  void ExpectNonIndexedLiteral(StringPiece name, StringPiece value) {
    expected_.AppendPrefix(kLiteralNoIndexOpcode);
    expected_.AppendUint32(0);
    expected_.AppendPrefix(kStringLiteralIdentityEncoded);
    expected_.AppendUint32(name.size());
    expected_.AppendBytes(name);
    expected_.AppendPrefix(kStringLiteralIdentityEncoded);
    expected_.AppendUint32(value.size());
    expected_.AppendBytes(value);
  }
  void CompareWithExpectedEncoding(const map<string, string>& header_set) {
    string expected_out, actual_out;
    expected_.TakeString(&expected_out);
    EXPECT_TRUE(encoder_.EncodeHeaderSet(header_set, &actual_out));
    EXPECT_EQ(expected_out, actual_out);
  }
  size_t IndexOf(HpackEntry* entry) {
    return peer_.table()->IndexOf(entry);
  }
  size_t IndexOf(const HpackEntry* entry) {
    return peer_.table()->IndexOf(entry);
  }

  HpackEncoder encoder_;
  test::HpackEncoderPeer peer_;

  const HpackEntry* static_;
  const HpackEntry* key_1_;
  const HpackEntry* key_2_;
  const HpackEntry* cookie_a_;
  const HpackEntry* cookie_c_;

  HpackOutputStream expected_;
};

TEST_F(HpackEncoderTest, SingleDynamicIndex) {
  ExpectIndex(IndexOf(key_2_));

  map<string, string> headers;
  headers[key_2_->name()] = key_2_->value();
  CompareWithExpectedEncoding(headers);
}

TEST_F(HpackEncoderTest, SingleStaticIndex) {
  ExpectIndex(IndexOf(static_));

  map<string, string> headers;
  headers[static_->name()] = static_->value();
  CompareWithExpectedEncoding(headers);
}

TEST_F(HpackEncoderTest, SingleStaticIndexTooLarge) {
  peer_.table()->SetMaxSize(1);  // Also evicts all fixtures.
  ExpectIndex(IndexOf(static_));

  map<string, string> headers;
  headers[static_->name()] = static_->value();
  CompareWithExpectedEncoding(headers);

  EXPECT_EQ(0u, peer_.table_peer().dynamic_entries()->size());
}

TEST_F(HpackEncoderTest, SingleLiteralWithIndexName) {
  ExpectIndexedLiteral(key_2_, "value3");

  map<string, string> headers;
  headers[key_2_->name()] = "value3";
  CompareWithExpectedEncoding(headers);

  // A new entry was inserted and added to the reference set.
  HpackEntry* new_entry = &peer_.table_peer().dynamic_entries()->front();
  EXPECT_EQ(new_entry->name(), key_2_->name());
  EXPECT_EQ(new_entry->value(), "value3");
}

TEST_F(HpackEncoderTest, SingleLiteralWithLiteralName) {
  ExpectIndexedLiteral("key3", "value3");

  map<string, string> headers;
  headers["key3"] = "value3";
  CompareWithExpectedEncoding(headers);

  HpackEntry* new_entry = &peer_.table_peer().dynamic_entries()->front();
  EXPECT_EQ(new_entry->name(), "key3");
  EXPECT_EQ(new_entry->value(), "value3");
}

TEST_F(HpackEncoderTest, SingleLiteralTooLarge) {
  peer_.table()->SetMaxSize(1);  // Also evicts all fixtures.

  ExpectIndexedLiteral("key3", "value3");

  // A header overflowing the header table is still emitted.
  // The header table is empty.
  map<string, string> headers;
  headers["key3"] = "value3";
  CompareWithExpectedEncoding(headers);

  EXPECT_EQ(0u, peer_.table_peer().dynamic_entries()->size());
}

TEST_F(HpackEncoderTest, EmitThanEvict) {
  // |key_1_| is toggled and placed into the reference set,
  // and then immediately evicted by "key3".
  ExpectIndex(IndexOf(key_1_));
  ExpectIndexedLiteral("key3", "value3");

  map<string, string> headers;
  headers[key_1_->name()] = key_1_->value();
  headers["key3"] = "value3";
  CompareWithExpectedEncoding(headers);
}

TEST_F(HpackEncoderTest, CookieHeaderIsCrumbled) {
  ExpectIndex(IndexOf(cookie_a_));
  ExpectIndex(IndexOf(cookie_c_));
  ExpectIndexedLiteral(peer_.table()->GetByName("cookie"), "e=ff");

  map<string, string> headers;
  headers["cookie"] = "e=ff; a=bb; c=dd";
  CompareWithExpectedEncoding(headers);
}

TEST_F(HpackEncoderTest, StringsDynamicallySelectHuffmanCoding) {
  peer_.set_allow_huffman_compression(true);

  // Compactable string. Uses Huffman coding.
  peer_.EmitString("feedbeef");
  expected_.AppendPrefix(kStringLiteralHuffmanEncoded);
  expected_.AppendUint32(6);
  expected_.AppendBytes("\x94\xA5\x92""2\x96_");

  // Non-compactable. Uses identity coding.
  peer_.EmitString("@@@@@@");
  expected_.AppendPrefix(kStringLiteralIdentityEncoded);
  expected_.AppendUint32(6);
  expected_.AppendBytes("@@@@@@");

  string expected_out, actual_out;
  expected_.TakeString(&expected_out);
  peer_.TakeString(&actual_out);
  EXPECT_EQ(expected_out, actual_out);
}

TEST_F(HpackEncoderTest, EncodingWithoutCompression) {
  // Implementation should internally disable.
  peer_.set_allow_huffman_compression(true);

  ExpectNonIndexedLiteral(":path", "/index.html");
  ExpectNonIndexedLiteral("cookie", "foo=bar; baz=bing");
  ExpectNonIndexedLiteral("hello", "goodbye");

  map<string, string> headers;
  headers[":path"] = "/index.html";
  headers["cookie"] = "foo=bar; baz=bing";
  headers["hello"] = "goodbye";

  string expected_out, actual_out;
  expected_.TakeString(&expected_out);
  encoder_.EncodeHeaderSetWithoutCompression(headers, &actual_out);
  EXPECT_EQ(expected_out, actual_out);
}

TEST_F(HpackEncoderTest, MultipleEncodingPasses) {
  // Pass 1.
  {
    map<string, string> headers;
    headers["key1"] = "value1";
    headers["cookie"] = "a=bb";

    ExpectIndex(IndexOf(cookie_a_));
    ExpectIndex(IndexOf(key_1_));
    CompareWithExpectedEncoding(headers);
  }
  // Header table is:
  // 65: key1: value1
  // 64: key2: value2
  // 63: cookie: a=bb
  // 62: cookie: c=dd
  // Pass 2.
  {
    map<string, string> headers;
    headers["key1"] = "value1";
    headers["key2"] = "value2";
    headers["cookie"] = "c=dd; e=ff";

    ExpectIndex(IndexOf(cookie_c_));
    // key1 by index.
    ExpectIndex(65);
    // key2 by index.
    ExpectIndex(64);
    // This cookie evicts |key1| from the header table.
    ExpectIndexedLiteral(peer_.table()->GetByName("cookie"), "e=ff");

    CompareWithExpectedEncoding(headers);
  }
  // Header table is:
  // 65: key2: value2
  // 64: cookie: a=bb
  // 63: cookie: c=dd
  // 62: cookie: e=ff
  // Pass 3.
  {
    map<string, string> headers;
    headers["key1"] = "value1";
    headers["key3"] = "value3";
    headers["cookie"] = "e=ff";

    // cookie: e=ff by index.
    ExpectIndex(62);
    ExpectIndexedLiteral("key1", "value1");
    ExpectIndexedLiteral("key3", "value3");

    CompareWithExpectedEncoding(headers);
  }
}

TEST_F(HpackEncoderTest, PseudoHeadersFirst) {
  map<string, string> headers;
  // A pseudo-header to be indexed.
  headers[":authority"] = "www.example.com";
  // A pseudo-header that should not be indexed.
  headers[":path"] = "/spam/eggs.html";
  // A regular header which precedes ":" alphabetically, should still be encoded
  // after pseudo-headers.
  headers["-foo"] = "bar";
  headers["foo"] = "bar";
  headers["cookie"] = "c=dd";

  // Pseudo-headers are encoded in alphabetical order.
  ExpectIndexedLiteral(peer_.table()->GetByName(":authority"),
                       "www.example.com");
  ExpectNonIndexedLiteral(":path", "/spam/eggs.html");
  // Regular headers in the header table are encoded first.
  ExpectIndex(IndexOf(cookie_a_));
  // Regular headers not in the header table are encoded, in alphabetical order.
  ExpectIndexedLiteral("-foo", "bar");
  ExpectIndexedLiteral("foo", "bar");
  CompareWithExpectedEncoding(headers);
}

TEST_F(HpackEncoderTest, CookieToCrumbs) {
  test::HpackEncoderPeer peer(NULL);
  std::vector<StringPiece> out;

  // A space after ';' is consumed. All other spaces remain. ';' at beginning
  // and end of string produce empty crumbs. Duplicate crumbs are removed.
  // See section 8.1.3.4 "Compressing the Cookie Header Field" in the HTTP/2
  // specification at http://tools.ietf.org/html/draft-ietf-httpbis-http2-11
  peer.CookieToCrumbs(" foo=1;bar=2 ; bar=3;  bing=4; ", &out);
  EXPECT_THAT(out, ElementsAre("", " bing=4", " foo=1", "bar=2 ", "bar=3"));

  peer.CookieToCrumbs(";;foo = bar ;; ;baz =bing", &out);
  EXPECT_THAT(out, ElementsAre("", "baz =bing", "foo = bar "));

  peer.CookieToCrumbs("baz=bing; foo=bar; baz=bing", &out);
  EXPECT_THAT(out, ElementsAre("baz=bing", "foo=bar"));

  peer.CookieToCrumbs("baz=bing", &out);
  EXPECT_THAT(out, ElementsAre("baz=bing"));

  peer.CookieToCrumbs("", &out);
  EXPECT_THAT(out, ElementsAre(""));

  peer.CookieToCrumbs("foo;bar; baz;baz;bing;", &out);
  EXPECT_THAT(out, ElementsAre("", "bar", "baz", "bing", "foo"));
}

TEST_F(HpackEncoderTest, UpdateCharacterCounts) {
  std::vector<size_t> counts(256, 0);
  size_t total_counts = 0;
  encoder_.SetCharCountsStorage(&counts, &total_counts);

  char kTestString[] = "foo\0\1\xff""boo";
  peer_.UpdateCharacterCounts(
      StringPiece(kTestString, arraysize(kTestString) - 1));

  std::vector<size_t> expect(256, 0);
  expect[static_cast<uint8>('f')] = 1;
  expect[static_cast<uint8>('o')] = 4;
  expect[static_cast<uint8>('\0')] = 1;
  expect[static_cast<uint8>('\1')] = 1;
  expect[static_cast<uint8>('\xff')] = 1;
  expect[static_cast<uint8>('b')] = 1;

  EXPECT_EQ(expect, counts);
  EXPECT_EQ(9u, total_counts);
}

TEST_F(HpackEncoderTest, DecomposeRepresentation) {
  test::HpackEncoderPeer peer(NULL);
  std::vector<StringPiece> out;

  peer.DecomposeRepresentation("", &out);
  EXPECT_THAT(out, ElementsAre(""));

  peer.DecomposeRepresentation("foobar", &out);
  EXPECT_THAT(out, ElementsAre("foobar"));

  peer.DecomposeRepresentation(StringPiece("foo\0bar", 7), &out);
  EXPECT_THAT(out, ElementsAre("foo", "bar"));

  peer.DecomposeRepresentation(StringPiece("\0foo\0bar", 8), &out);
  EXPECT_THAT(out, ElementsAre("", "foo", "bar"));

  peer.DecomposeRepresentation(StringPiece("foo\0bar\0", 8), &out);
  EXPECT_THAT(out, ElementsAre("foo", "bar", ""));

  peer.DecomposeRepresentation(StringPiece("\0foo\0bar\0", 9), &out);
  EXPECT_THAT(out, ElementsAre("", "foo", "bar", ""));
}

// Test that encoded headers do not have \0-delimited multiple values, as this
// became disallowed in HTTP/2 draft-14.
TEST_F(HpackEncoderTest, CrumbleNullByteDelimitedValue) {
  map<string, string> headers;
  // A header field to be crumbled: "spam: foo\0bar".
  headers["spam"] = string("foo\0bar", 7);

  ExpectIndexedLiteral("spam", "foo");
  expected_.AppendPrefix(kLiteralIncrementalIndexOpcode);
  expected_.AppendUint32(62);
  expected_.AppendPrefix(kStringLiteralIdentityEncoded);
  expected_.AppendUint32(3);
  expected_.AppendBytes("bar");
  CompareWithExpectedEncoding(headers);
}

}  // namespace

}  // namespace net
