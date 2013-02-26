// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_frame_builder.h"

#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_protocol.h"
#include "testing/platform_test.h"

namespace net {

TEST(SpdyFrameBuilderTestVersionAgnostic, GetWritableBuffer) {
  const size_t builder_size = 10;
  SpdyFrameBuilder builder(builder_size);
  char* writable_buffer = builder.GetWritableBuffer(builder_size);
  memset(writable_buffer, ~1, builder_size);
  EXPECT_TRUE(builder.Seek(builder_size));
  scoped_ptr<SpdyFrame> frame(builder.take());
  char expected[builder_size];
  memset(expected, ~1, builder_size);
  EXPECT_EQ(base::StringPiece(expected, builder_size),
            base::StringPiece(frame->data(), builder_size));
}

enum SpdyFrameBuilderTestTypes {
  SPDY2 = 2,
  SPDY3 = 3,
};

class SpdyFrameBuilderTest
    : public ::testing::TestWithParam<SpdyFrameBuilderTestTypes> {
 protected:
  virtual void SetUp() {
    spdy_version_ = GetParam();
  }

  // Version of SPDY protocol to be used.
  unsigned char spdy_version_;
};

// All tests are run with two different SPDY versions: SPDY/2 and SPDY/3.
INSTANTIATE_TEST_CASE_P(SpdyFrameBuilderTests,
                        SpdyFrameBuilderTest,
                        ::testing::Values(SPDY2, SPDY3));

TEST_P(SpdyFrameBuilderTest, RewriteLength) {
  // Create an empty SETTINGS frame both via framer and manually via builder.
  // The one created via builder is initially given the incorrect length, but
  // then is corrected via RewriteLength().
  SpdyFramer framer(spdy_version_);
  SettingsMap settings;
  scoped_ptr<SpdyFrame> expected(framer.CreateSettings(settings));
  SpdyFrameBuilder builder(SETTINGS, 0, spdy_version_, expected->size() + 1);
  builder.WriteUInt32(0); // Write the number of settings.
  EXPECT_TRUE(builder.GetWritableBuffer(1) != NULL);
  builder.RewriteLength(framer);
  scoped_ptr<SpdyFrame> built(builder.take());
  EXPECT_EQ(base::StringPiece(expected->data(), expected->size()),
            base::StringPiece(built->data(), expected->size()));
}

}  // namespace net
