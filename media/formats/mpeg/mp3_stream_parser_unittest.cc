// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/common/stream_parser_test_base.h"
#include "media/formats/mpeg/mp3_stream_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class MP3StreamParserTest : public StreamParserTestBase, public testing::Test {
 public:
  MP3StreamParserTest()
      : StreamParserTestBase(
            scoped_ptr<StreamParser>(new MP3StreamParser()).Pass()) {}
  virtual ~MP3StreamParserTest() {}
};

// Test parsing with small prime sized chunks to smoke out "power of
// 2" field size assumptions.
TEST_F(MP3StreamParserTest, UnalignedAppend) {
  const std::string expected =
      "NewSegment"
      "{ 0K }"
      "{ 0K }"
      "{ 0K }"
      "{ 0K }"
      "{ 0K }"
      "{ 0K }"
      "{ 0K }"
      "{ 0K }"
      "EndOfSegment"
      "NewSegment"
      "{ 0K }"
      "{ 0K }"
      "{ 0K }"
      "EndOfSegment"
      "NewSegment"
      "{ 0K }"
      "{ 0K }"
      "EndOfSegment";
  EXPECT_EQ(expected, ParseFile("sfx.mp3", 17));
}

// Test parsing with a larger piece size to verify that multiple buffers
// are passed to |new_buffer_cb_|.
TEST_F(MP3StreamParserTest, UnalignedAppend512) {
  const std::string expected =
      "NewSegment"
      "{ 0K }"
      "{ 0K 26K 52K 78K }"
      "EndOfSegment"
      "NewSegment"
      "{ 0K 26K 52K }"
      "{ 0K 26K 52K 78K }"
      "{ 0K }"
      "EndOfSegment";
  EXPECT_EQ(expected, ParseFile("sfx.mp3", 512));
}

}  // namespace media
