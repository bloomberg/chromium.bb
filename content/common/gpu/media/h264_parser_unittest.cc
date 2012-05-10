// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "content/common/gpu/media/h264_parser.h"

using content::H264Parser;
using content::H264NALU;

const FilePath::CharType* test_stream_filename =
    FILE_PATH_LITERAL("content/common/gpu/testdata/test-25fps.h264");
// Number of NALUs in the stream to be parsed.
int num_nalus = 759;

TEST(H264ParserTest, StreamFileParsing) {
  FilePath fp(test_stream_filename);
  file_util::MemoryMappedFile stream;
  CHECK(stream.Initialize(fp)) << "Couldn't open stream file: "
                               << test_stream_filename;
  DVLOG(1) << "Parsing file: " << test_stream_filename;

  H264Parser parser;
  parser.SetStream(stream.data(), stream.length());

  // Parse until the end of stream/unsupported stream/error in stream is found.
  int num_parsed_nalus = 0;
  while (true) {
    content::H264SliceHeader shdr;
    content::H264SEIMessage sei_msg;
    H264NALU nalu;
    H264Parser::Result res = parser.AdvanceToNextNALU(&nalu);
    if (res == H264Parser::kEOStream) {
      DVLOG(1) << "Number of successfully parsed NALUs before EOS: "
               << num_parsed_nalus;
      ASSERT_EQ(num_nalus, num_parsed_nalus);
      return;
    }
    ASSERT_EQ(res, H264Parser::kOk);

    ++num_parsed_nalus;

    int id;
    switch (nalu.nal_unit_type) {
      case H264NALU::kIDRSlice:
      case H264NALU::kNonIDRSlice:
        ASSERT_EQ(parser.ParseSliceHeader(nalu, &shdr), H264Parser::kOk);
        break;

      case H264NALU::kSPS:
        ASSERT_EQ(parser.ParseSPS(&id), H264Parser::kOk);
        break;

      case H264NALU::kPPS:
        ASSERT_EQ(parser.ParsePPS(&id), H264Parser::kOk);
        break;

      case H264NALU::kSEIMessage:
        ASSERT_EQ(parser.ParseSEI(&sei_msg), H264Parser::kOk);
        break;

      default:
        // Skip unsupported NALU.
        DVLOG(4) << "Skipping unsupported NALU";
        break;
    }
  }
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  CommandLine::Init(argc, argv);

  const CommandLine::SwitchMap& switches =
      CommandLine::ForCurrentProcess()->GetSwitches();
  for (CommandLine::SwitchMap::const_iterator it = switches.begin();
       it != switches.end(); ++it) {
    if (it->first == "test_stream") {
      test_stream_filename = it->second.c_str();
    } else if (it->first == "num_nalus") {
      CHECK(base::StringToInt(it->second, &num_nalus));
    } else {
      LOG(FATAL) << "Unexpected switch: " << it->first << ":" << it->second;
    }
  }

  return RUN_ALL_TESTS();
}

