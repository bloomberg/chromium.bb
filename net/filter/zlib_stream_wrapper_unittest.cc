// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/filter/zlib_stream_wrapper.h"
#include "base/memory/scoped_refptr.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/filter/filter_source_stream_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

class ZLibStreamWrapperTest
    : public testing::Test,
      public testing::WithParamInterface<ZLibStreamWrapper::SourceType> {
 public:
  ZLibStreamWrapper::SourceType source_type() { return GetParam(); }

  bool compress_with_gzip_framing() {
    return GetParam() == ZLibStreamWrapper::SourceType::kGzip;
  }
};

TEST_P(ZLibStreamWrapperTest, SmallInflate) {
  const std::string uncompressed_data = "hello";

  size_t compressed_data_len = 2048;
  auto compressed_data = std::make_unique<char[]>(compressed_data_len);
  net::CompressGzip(uncompressed_data.data(), uncompressed_data.size(),
                    compressed_data.get(), &compressed_data_len,
                    compress_with_gzip_framing());

  ZLibStreamWrapper zlib_wrapper(source_type());

  ASSERT_TRUE(zlib_wrapper.Init());

  scoped_refptr<WrappedIOBuffer> input_buffer =
      new WrappedIOBuffer(compressed_data.get());
  scoped_refptr<IOBuffer> output_buffer =
      new IOBuffer(uncompressed_data.size());
  int consumed_bytes(0);
  int bytes_written = zlib_wrapper.FilterData(
      output_buffer.get(), uncompressed_data.size(), input_buffer.get(),
      compressed_data_len, &consumed_bytes,
      /*upstream_end_reached=*/true);

  ASSERT_EQ(static_cast<int>(uncompressed_data.size()), bytes_written);
  EXPECT_EQ(static_cast<int>(compressed_data_len), consumed_bytes);
  EXPECT_EQ(0, memcmp(output_buffer->data(), uncompressed_data.data(),
                      uncompressed_data.size()));
}

TEST_P(ZLibStreamWrapperTest, SmallCorruptedInflate) {
  const std::string uncompressed_data = "hello";

  size_t compressed_data_len = 2048;
  auto compressed_data = std::make_unique<char[]>(compressed_data_len);
  net::CompressGzip(uncompressed_data.data(), uncompressed_data.size(),
                    compressed_data.get(), &compressed_data_len,
                    compress_with_gzip_framing());

  // Corrupt the compressed data stream.
  compressed_data[1] = 'X';

  ZLibStreamWrapper zlib_wrapper(source_type());

  ASSERT_TRUE(zlib_wrapper.Init());

  scoped_refptr<WrappedIOBuffer> input_buffer =
      new WrappedIOBuffer(compressed_data.get());
  scoped_refptr<IOBuffer> output_buffer =
      new IOBuffer(uncompressed_data.size());
  int consumed_bytes(0);
  int bytes_written = zlib_wrapper.FilterData(
      output_buffer.get(), uncompressed_data.size(), input_buffer.get(),
      compressed_data_len, &consumed_bytes,
      /*upstream_end_reached=*/true);

  EXPECT_EQ(net::ERR_CONTENT_DECODING_FAILED, bytes_written);
  EXPECT_EQ(0, consumed_bytes);
}

INSTANTIATE_TEST_CASE_P(
    ,
    ZLibStreamWrapperTest,
    testing::Values(ZLibStreamWrapper::SourceType::kGzip,
                    ZLibStreamWrapper::SourceType::kDeflate));
}  // namespace net
