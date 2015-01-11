// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_byteorder.h"
#include "media/base/test_data_util.h"
#include "media/filters/vp8_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(Vp8ParserTest, StreamFileParsing) {
  base::FilePath file_path = GetTestDataFilePath("test-25fps.vp8");
  // Number of frames in the test stream to be parsed.
  const int num_frames = 250;

  base::MemoryMappedFile stream;
  ASSERT_TRUE(stream.Initialize(file_path))
      << "Couldn't open stream file: " << file_path.MaybeAsASCII();

  Vp8Parser parser;

  // Parse until the end of stream/unsupported stream/error in stream is found.
  int num_parsed_frames = 0;
  const uint8_t* stream_ptr = stream.data();
  size_t bytes_left = stream.length();
  // Skip IVF file header.
  const size_t kIvfStreamHeaderLen = 32;
  CHECK_GE(bytes_left, kIvfStreamHeaderLen);
  stream_ptr += kIvfStreamHeaderLen;
  bytes_left -= kIvfStreamHeaderLen;

  const size_t kIvfFrameHeaderLen = 12;
  while (bytes_left > kIvfFrameHeaderLen) {
    Vp8FrameHeader fhdr;
    uint32_t frame_size =
        base::ByteSwapToLE32(*reinterpret_cast<const uint32_t*>(stream_ptr));
    // Skip IVF frame header.
    stream_ptr += kIvfFrameHeaderLen;
    bytes_left -= kIvfFrameHeaderLen;

    ASSERT_TRUE(parser.ParseFrame(stream_ptr, frame_size, &fhdr));

    stream_ptr += frame_size;
    bytes_left -= frame_size;
    ++num_parsed_frames;
  }

  DVLOG(1) << "Number of successfully parsed frames before EOS: "
           << num_parsed_frames;

  EXPECT_EQ(num_frames, num_parsed_frames);
}

}  // namespace media
