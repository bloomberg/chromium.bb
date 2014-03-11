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

class HpackEncoderPeer {
 public:
  explicit HpackEncoderPeer(HpackEncoder* encoder)
      : encoder_(encoder) {}

  void set_max_string_literal_size(uint32 size) {
    encoder_->max_string_literal_size_ = size;
  }
  static void CookieToCrumbs(StringPiece cookie,
                             std::vector<StringPiece>* out) {
    HpackEncoder::CookieToCrumbs(cookie, out);
  }
 private:
  HpackEncoder* encoder_;
};

}  // namespace test

namespace {

TEST(HpackEncoderTest, CookieToCrumbs) {
  test::HpackEncoderPeer peer(NULL);
  std::vector<StringPiece> out;

  // A space after ';' is consumed. All other spaces remain.
  // ';' at begin and end of string produce empty crumbs.
  peer.CookieToCrumbs(" foo=1;bar=2 ; baz=3;  bing=4;", &out);
  EXPECT_THAT(out, ElementsAre(" foo=1", "bar=2 ", "baz=3", " bing=4", ""));

  peer.CookieToCrumbs(";;foo = bar ;; ;baz =bing", &out);
  EXPECT_THAT(out, ElementsAre("", "", "foo = bar ", "", "", "baz =bing"));

  peer.CookieToCrumbs("foo=bar; baz=bing", &out);
  EXPECT_THAT(out, ElementsAre("foo=bar", "baz=bing"));

  peer.CookieToCrumbs("baz=bing", &out);
  EXPECT_THAT(out, ElementsAre("baz=bing"));

  peer.CookieToCrumbs("", &out);
  EXPECT_THAT(out, ElementsAre(""));
}

// Test that EncoderHeaderSet() simply encodes everything as literals
// without indexing.
TEST(HpackEncoderTest, Basic) {
  HpackEncoder encoder;

  std::map<string, string> header_set1;
  header_set1["name1"] = "value1";
  header_set1["name2"] = "value2";

  string encoded_header_set1;
  EXPECT_TRUE(encoder.EncodeHeaderSet(header_set1, &encoded_header_set1));
  EXPECT_EQ("\x40\x05name1\x06value1"
            "\x40\x05name2\x06value2", encoded_header_set1);

  std::map<string, string> header_set2;
  header_set2["name2"] = "different-value";
  header_set2["name3"] = "value3";

  string encoded_header_set2;
  EXPECT_TRUE(encoder.EncodeHeaderSet(header_set2, &encoded_header_set2));
  EXPECT_EQ("\x40\x05name2\x0f" "different-value"
            "\x40\x05name3\x06value3", encoded_header_set2);
}

TEST(HpackEncoderTest, CookieCrumbling) {
  HpackEncoder encoder;

  std::map<string, string> header_set;
  header_set["Cookie"] = "key1=value1; key2=value2";

  string encoded_header_set;
  EXPECT_TRUE(encoder.EncodeHeaderSet(header_set, &encoded_header_set));
  EXPECT_EQ("\x40\x06""Cookie\x0bkey1=value1"
            "\x40\x06""Cookie\x0bkey2=value2", encoded_header_set);
}

// Test that trying to encode a header set with a too-long header
// field will fail.
TEST(HpackEncoderTest, HeaderTooLarge) {
  HpackEncoder encoder;
  test::HpackEncoderPeer(&encoder).set_max_string_literal_size(10);

  std::map<string, string> header_set;
  header_set["name1"] = "too-long value";
  header_set["name2"] = "value2";

  // TODO(akalin): Verify that the encoder did not attempt to encode
  // the second header field.
  string encoded_header_set;
  EXPECT_FALSE(encoder.EncodeHeaderSet(header_set, &encoded_header_set));
}

}  // namespace

}  // namespace net
