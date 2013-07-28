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

class QuicSpdyDecompressorTest : public ::testing::Test {
 protected:
  QuicSpdyDecompressor decompressor_;
  QuicSpdyCompressor compressor_;
  TestDecompressorVisitor visitor_;
};

TEST_F(QuicSpdyDecompressorTest, Decompress) {
  SpdyHeaderBlock headers;
  headers[":host"] = "www.google.com";
  headers[":path"] = "/index.hml";
  headers[":scheme"] = "https";

  EXPECT_EQ(1u, decompressor_.current_header_id());
  string compressed_headers = compressor_.CompressHeaders(headers).substr(4);
  EXPECT_EQ(compressed_headers.length(),
            decompressor_.DecompressData(compressed_headers, &visitor_));

  EXPECT_EQ(SpdyUtils::SerializeUncompressedHeaders(headers), visitor_.data());
  EXPECT_EQ(2u, decompressor_.current_header_id());
}

TEST_F(QuicSpdyDecompressorTest, DecompressPartial) {
  SpdyHeaderBlock headers;
  headers[":host"] = "www.google.com";
  headers[":path"] = "/index.hml";
  headers[":scheme"] = "https";
  string compressed_headers = compressor_.CompressHeaders(headers).substr(4);

  for (size_t i = 0; i < compressed_headers.length(); ++i) {
    QuicSpdyDecompressor decompressor;
    TestDecompressorVisitor visitor;

    EXPECT_EQ(1u, decompressor.current_header_id());

    string partial_compressed_headers = compressed_headers.substr(0, i);
    EXPECT_EQ(partial_compressed_headers.length(),
              decompressor.DecompressData(partial_compressed_headers,
                                          &visitor));
    EXPECT_EQ(1u, decompressor.current_header_id()) << "i: " << i;

    string remaining_compressed_headers =
        compressed_headers.substr(partial_compressed_headers.length());
    EXPECT_EQ(remaining_compressed_headers.length(),
              decompressor.DecompressData(remaining_compressed_headers,
                                          &visitor));
    EXPECT_EQ(SpdyUtils::SerializeUncompressedHeaders(headers), visitor.data());

    EXPECT_EQ(2u, decompressor.current_header_id());
  }
}

TEST_F(QuicSpdyDecompressorTest, DecompressAndIgnoreTrailingData) {
  SpdyHeaderBlock headers;
  headers[":host"] = "www.google.com";
  headers[":path"] = "/index.hml";
  headers[":scheme"] = "https";

  string compressed_headers = compressor_.CompressHeaders(headers).substr(4);
  EXPECT_EQ(compressed_headers.length(),
            decompressor_.DecompressData(compressed_headers + "abc123",
                                        &visitor_));

  EXPECT_EQ(SpdyUtils::SerializeUncompressedHeaders(headers), visitor_.data());
}

TEST_F(QuicSpdyDecompressorTest, DecompressError) {
  SpdyHeaderBlock headers;
  headers[":host"] = "www.google.com";
  headers[":path"] = "/index.hml";
  headers[":scheme"] = "https";

  EXPECT_EQ(1u, decompressor_.current_header_id());
  string compressed_headers = compressor_.CompressHeaders(headers).substr(4);
  compressed_headers[compressed_headers.length() - 1] ^= 0x01;
  EXPECT_NE(compressed_headers.length(),
            decompressor_.DecompressData(compressed_headers, &visitor_));

  EXPECT_TRUE(visitor_.error());
  EXPECT_EQ("", visitor_.data());
}

}  // namespace
}  // namespace test
}  // namespace net
