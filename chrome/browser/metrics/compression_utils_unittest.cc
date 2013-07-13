// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/base_paths.h"
#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/metrics/compression_utils.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// The data to be compressed by gzip. This is the hex representation of "hello
// world".
const uint8 kData[] =
    {0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x77, 0x6f,
     0x72, 0x6c, 0x64};

// This is the string representation of gzip compressed string above. It was
// obtained by running echo -n "hello world" | gzip -c | hexdump -e '8 1 ",
// 0x%x"' followed by 0'ing out the OS byte (10th byte) in the header. This is
// so that the test passes on all platforms (that run various OS'es).
const uint8 kCompressedData[] =
    {0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0xcb, 0x48, 0xcd, 0xc9, 0xc9, 0x57, 0x28, 0xcf,
     0x2f, 0xca, 0x49, 0x01, 0x00, 0x85, 0x11, 0x4a, 0x0d,
     0x0b, 0x00, 0x00, 0x00};

// Re-enable C4309.
#if defined(OS_WIN)
#pragma warning( default: 4309 )
#endif

TEST(CompressionUtilsTest, GzipCompression) {
  std::string data(reinterpret_cast<const char*>(kData), arraysize(kData));
  std::string compressed_data;
  EXPECT_TRUE(chrome::GzipCompress(data, &compressed_data));
  std::string golden_compressed_data(
      reinterpret_cast<const char*>(kCompressedData),
      arraysize(kCompressedData));
  EXPECT_EQ(golden_compressed_data, compressed_data);
}

}  // namespace
