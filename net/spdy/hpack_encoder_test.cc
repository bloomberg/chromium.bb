// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/hpack_encoder.h"

#include <map>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

using std::string;

// Test that EncoderHeaderSet() simply encodes everything as literals
// without indexing.
TEST(HpackEncoderTest, Basic) {
  HpackEncoder encoder(kuint32max);

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

// Test that trying to encode a header set with a too-long header
// field will fail.
TEST(HpackEncoderTest, HeaderTooLarge) {
  HpackEncoder encoder(10);

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
