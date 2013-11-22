// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/test_data_util.h"
#include "media/base/text_track_config.h"
#include "media/base/video_decoder_config.h"
#include "media/mp3/mp3_stream_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class MP3StreamParserTest : public testing::Test {
 public:
  MP3StreamParserTest() {}

 protected:
  MP3StreamParser parser_;
  std::stringstream results_stream_;

  bool AppendData(const uint8* data, size_t length) {
    return parser_.Parse(data, length);
  }

  bool AppendDataInPieces(const uint8* data, size_t length, size_t piece_size) {
    const uint8* start = data;
    const uint8* end = data + length;
    while (start < end) {
      size_t append_size =
          std::min(piece_size, static_cast<size_t>(end - start));
      if (!AppendData(start, append_size))
        return false;
      start += append_size;
    }
    return true;
  }

  void OnInitDone(bool success, base::TimeDelta duration) {
    DVLOG(1) << __FUNCTION__ << "(" << success << ", "
             << duration.InMilliseconds() << ")";
  }

  bool OnNewConfig(const AudioDecoderConfig& audio_config,
                   const VideoDecoderConfig& video_config,
                   const StreamParser::TextTrackConfigMap& text_config) {
    DVLOG(1) << __FUNCTION__ << "(" << audio_config.IsValidConfig() << ", "
             << video_config.IsValidConfig() << ")";
    EXPECT_TRUE(audio_config.IsValidConfig());
    EXPECT_FALSE(video_config.IsValidConfig());
    return true;
  }

  std::string BufferQueueToString(const StreamParser::BufferQueue& buffers) {
    std::stringstream ss;

    ss << "{";
    for (StreamParser::BufferQueue::const_iterator itr = buffers.begin();
         itr != buffers.end();
         ++itr) {
      ss << " " << (*itr)->timestamp().InMilliseconds();
      if ((*itr)->IsKeyframe())
        ss << "K";
    }
    ss << " }";

    return ss.str();
  }

  bool OnNewBuffers(const StreamParser::BufferQueue& audio_buffers,
                    const StreamParser::BufferQueue& video_buffers) {
    EXPECT_FALSE(audio_buffers.empty());
    EXPECT_TRUE(video_buffers.empty());

    std::string buffers_str = BufferQueueToString(audio_buffers);
    DVLOG(1) << __FUNCTION__ << " : " << buffers_str;
    results_stream_ << buffers_str;
    return true;
  }

  void OnKeyNeeded(const std::string& type,
                   const std::vector<uint8>& init_data) {
    DVLOG(1) << __FUNCTION__ << "(" << type << ", " << init_data.size() << ")";
  }

  void OnNewSegment() {
    DVLOG(1) << __FUNCTION__;
    results_stream_ << "NewSegment";
  }

  void OnEndOfSegment() {
    DVLOG(1) << __FUNCTION__;
    results_stream_ << "EndOfSegment";
  }

  void InitializeParser() {
    parser_.Init(
        base::Bind(&MP3StreamParserTest::OnInitDone, base::Unretained(this)),
        base::Bind(&MP3StreamParserTest::OnNewConfig, base::Unretained(this)),
        base::Bind(&MP3StreamParserTest::OnNewBuffers, base::Unretained(this)),
        StreamParser::NewTextBuffersCB(),
        base::Bind(&MP3StreamParserTest::OnKeyNeeded, base::Unretained(this)),
        base::Bind(&MP3StreamParserTest::OnNewSegment, base::Unretained(this)),
        base::Bind(&MP3StreamParserTest::OnEndOfSegment,
                   base::Unretained(this)),
        LogCB());
  }

  std::string ParseFile(const std::string& filename, int append_bytes) {
    results_stream_.clear();
    InitializeParser();

    scoped_refptr<DecoderBuffer> buffer = ReadTestDataFile(filename);
    EXPECT_TRUE(
        AppendDataInPieces(buffer->data(), buffer->data_size(), append_bytes));
    return results_stream_.str();
  }
};

// Test parsing with small prime sized chunks to smoke out "power of
// 2" field size assumptions.
TEST_F(MP3StreamParserTest, UnalignedAppend) {
  std::string expected =
      "NewSegment"
      "{ 0K }"
      "{ 26K }"
      "{ 52K }"
      "{ 78K }"
      "{ 104K }"
      "{ 130K }"
      "{ 156K }"
      "{ 182K }"
      "EndOfSegment"
      "NewSegment"
      "{ 208K }"
      "{ 235K }"
      "{ 261K }"
      "EndOfSegment"
      "NewSegment"
      "{ 287K }"
      "{ 313K }"
      "EndOfSegment";
  EXPECT_EQ(expected, ParseFile("sfx.mp3", 17));
}

// Test parsing with a larger piece size to verify that multiple buffers
// are passed to |new_buffer_cb_|.
TEST_F(MP3StreamParserTest, UnalignedAppend512) {
  std::string expected =
      "NewSegment"
      "{ 0K }"
      "{ 26K 52K 78K 104K }"
      "EndOfSegment"
      "NewSegment"
      "{ 130K 156K 182K }"
      "{ 208K 235K 261K 287K }"
      "{ 313K }"
      "EndOfSegment";
  EXPECT_EQ(expected, ParseFile("sfx.mp3", 512));
}

}  // namespace media
