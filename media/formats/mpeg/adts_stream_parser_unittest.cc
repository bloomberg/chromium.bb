// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/common/stream_parser_test_base.h"
#include "media/formats/mpeg/adts_stream_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class ADTSStreamParserTest : public StreamParserTestBase, public testing::Test {
 public:
  ADTSStreamParserTest()
      : StreamParserTestBase(
            scoped_ptr<StreamParser>(new ADTSStreamParser()).Pass()) {}
  virtual ~ADTSStreamParserTest() {}
};

// Test parsing with small prime sized chunks to smoke out "power of
// 2" field size assumptions.
TEST_F(ADTSStreamParserTest, UnalignedAppend) {
  const std::string expected =
      "NewSegment"
      "{ 0K }"
      "{ 23K }"
      "{ 46K }"
      "{ 69K }"
      "{ 92K }"
      "{ 116K }"
      "{ 139K }"
      "{ 162K }"
      "{ 185K }"
      "EndOfSegment"
      "NewSegment"
      "{ 208K }"
      "{ 232K }"
      "{ 255K }"
      "{ 278K }"
      "{ 301K }"
      "EndOfSegment";
  EXPECT_EQ(expected, ParseFile("sfx.adts", 17));
}

// Test parsing with a larger piece size to verify that multiple buffers
// are passed to |new_buffer_cb_|.
TEST_F(ADTSStreamParserTest, UnalignedAppend512) {
  const std::string expected =
      "NewSegment"
      "{ 0K 23K 46K }"
      "{ 69K 92K 116K 139K 162K }"
      "{ 185K 208K 232K 255K 278K }"
      "EndOfSegment"
      "NewSegment"
      "{ 301K }"
      "EndOfSegment";
  EXPECT_EQ(expected, ParseFile("sfx.adts", 512));
}

}  // namespace media
