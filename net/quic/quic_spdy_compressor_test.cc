// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "net/quic/quic_spdy_compressor.h"
#include "net/quic/quic_spdy_decompressor.h"
#include "net/quic/spdy_utils.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::string;

namespace net {
namespace test {
namespace {

class QuicSpdyCompressorTest : public ::testing::Test {
 protected:
  TestDecompressorVisitor visitor_;
};

TEST_F(QuicSpdyCompressorTest, Compress) {
  QuicSpdyCompressor compressor;
  QuicSpdyDecompressor decompressor;

  SpdyHeaderBlock headers;
  headers[":host"] = "www.google.com";
  headers[":path"] = "/index.hml";
  headers[":scheme"] = "https";

  string compressed_headers = compressor.CompressHeaders(headers);
  EXPECT_EQ('\1', compressed_headers[0]);
  EXPECT_EQ('\0', compressed_headers[1]);
  EXPECT_EQ('\0', compressed_headers[2]);
  EXPECT_EQ('\0', compressed_headers[3]);
  string compressed_data = compressed_headers.substr(4);
  decompressor.DecompressData(compressed_data, &visitor_);
  EXPECT_EQ(SpdyUtils::SerializeUncompressedHeaders(headers),
            visitor_.data());
}

}  // namespace
}  // namespace test
}  // namespace net
