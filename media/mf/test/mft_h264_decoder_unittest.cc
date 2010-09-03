// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/time.h"
#include "media/base/data_buffer.h"
#include "media/base/video_frame.h"
#include "media/mf/file_reader_util.h"
#include "media/mf/mft_h264_decoder.h"
#include "media/video/video_decode_engine.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;

namespace media {

static const int kDecoderMaxWidth = 1920;
static const int kDecoderMaxHeight = 1088;

class BaseMftReader : public base::RefCountedThreadSafe<BaseMftReader> {
 public:
  virtual ~BaseMftReader() {}
  virtual void ReadCallback(scoped_refptr<DataBuffer>* input) = 0;
};

class FakeMftReader : public BaseMftReader {
 public:
  FakeMftReader() : frames_remaining_(20) {}
  explicit FakeMftReader(int count) : frames_remaining_(count) {}
  virtual ~FakeMftReader() {}

  // Provides garbage input to the decoder.
  virtual void ReadCallback(scoped_refptr<DataBuffer>* input) {
    if (frames_remaining_ > 0) {
      int sz = 4096;
      uint8* buf = new uint8[sz];
      memset(buf, 42, sz);
      *input = new DataBuffer(buf, sz);
      (*input)->SetDuration(base::TimeDelta::FromMicroseconds(5000));
      (*input)->SetTimestamp(
          base::TimeDelta::FromMicroseconds(
              50000000 - frames_remaining_ * 10000));
      --frames_remaining_;
    } else {
      // Emulate end of stream on the last "frame".
      *input = new DataBuffer(0);
    }
  }
  int frames_remaining() const { return frames_remaining_; }

 private:
  int frames_remaining_;
};

class FFmpegFileReaderWrapper : public BaseMftReader {
 public:
  FFmpegFileReaderWrapper() {}
  virtual ~FFmpegFileReaderWrapper() {}
  bool InitReader(const std::string& filename) {
    reader_.reset(new FFmpegFileReader(filename));
    if (!reader_.get() || !reader_->Initialize()) {
      reader_.reset();
      return false;
    }
    return true;
  }
  virtual void ReadCallback(scoped_refptr<DataBuffer>* input) {
    if (reader_.get()) {
      reader_->Read(input);
    }
  }
  bool GetWidth(int* width) {
    if (!reader_.get())
      return false;
    return reader_->GetWidth(width);
  }
  bool GetHeight(int* height) {
    if (!reader_.get())
      return false;
    return reader_->GetHeight(height);
  }
  scoped_ptr<FFmpegFileReader> reader_;
};

class MftH264DecoderTest : public testing::Test {
 public:
  MftH264DecoderTest() {}
  virtual ~MftH264DecoderTest() {}

 protected:
  virtual void SetUp() {}
  virtual void TearDown() {}
};

class SimpleMftH264DecoderHandler : public VideoDecodeEngine::EventHandler {
 public:
  SimpleMftH264DecoderHandler()
      : init_count_(0),
        uninit_count_(0),
        flush_count_(0),
        format_change_count_(0),
        empty_buffer_callback_count_(0),
        fill_buffer_callback_count_(0) {
    memset(&info_, 0, sizeof(info_));
  }
  virtual ~SimpleMftH264DecoderHandler() {}
  virtual void OnInitializeComplete(const VideoCodecInfo& info) {
    info_ = info;
    init_count_++;
  }
  virtual void OnUninitializeComplete() {
    uninit_count_++;
  }
  virtual void OnFlushComplete() {
    flush_count_++;
  }
  virtual void OnSeekComplete() {}
  virtual void OnError() {}
  virtual void OnFormatChange(VideoStreamInfo stream_info) {
    format_change_count_++;
    info_.stream_info_ = stream_info;
  }
  virtual void OnEmptyBufferCallback(scoped_refptr<Buffer> buffer) {
    if (reader_.get() && decoder_) {
      empty_buffer_callback_count_++;
      scoped_refptr<DataBuffer> input;
      reader_->ReadCallback(&input);
      decoder_->EmptyThisBuffer(input);
    }
  }
  virtual void OnFillBufferCallback(scoped_refptr<VideoFrame> frame) {
    fill_buffer_callback_count_++;
    current_frame_ = frame;
  }
  void SetReader(scoped_refptr<BaseMftReader> reader) {
    reader_ = reader;
  }
  void SetDecoder(MftH264Decoder* decoder) {
    decoder_ = decoder;
  }

  int init_count_;
  int uninit_count_;
  int flush_count_;
  int format_change_count_;
  int empty_buffer_callback_count_;
  int fill_buffer_callback_count_;
  VideoCodecInfo info_;
  scoped_refptr<BaseMftReader> reader_;
  MftH264Decoder* decoder_;
  scoped_refptr<VideoFrame> current_frame_;
};

// A simple test case for init/deinit of MF/COM libraries.
TEST_F(MftH264DecoderTest, LibraryInit) {
  EXPECT_TRUE(MftH264Decoder::StartupComLibraries());
  MftH264Decoder::ShutdownComLibraries();
}

TEST_F(MftH264DecoderTest, DecoderUninitializedAtFirst) {
  scoped_ptr<MftH264Decoder> decoder(new MftH264Decoder(true));
  ASSERT_TRUE(decoder.get());
  EXPECT_EQ(MftH264Decoder::kUninitialized, decoder->state());
}

TEST_F(MftH264DecoderTest, DecoderInitMissingArgs) {
  VideoCodecConfig config;
  config.width_ = 800;
  config.height_ = 600;
  scoped_ptr<MftH264Decoder> decoder(new MftH264Decoder(false));
  ASSERT_TRUE(decoder.get());
  decoder->Initialize(NULL, NULL, config);
  EXPECT_EQ(MftH264Decoder::kUninitialized, decoder->state());
}

TEST_F(MftH264DecoderTest, DecoderInitNoDxva) {
  MessageLoop loop;
  SimpleMftH264DecoderHandler handler;
  VideoCodecConfig config;
  config.width_ = 800;
  config.height_ = 600;
  scoped_ptr<MftH264Decoder> decoder(new MftH264Decoder(false));
  ASSERT_TRUE(decoder.get());
  decoder->Initialize(&loop, &handler, config);
  EXPECT_EQ(1, handler.init_count_);
  EXPECT_EQ(MftH264Decoder::kNormal, decoder->state());
  decoder->Uninitialize();
}

TEST_F(MftH264DecoderTest, DecoderInitDxva) {
  MessageLoop loop;
  SimpleMftH264DecoderHandler handler;
  VideoCodecConfig config;
  config.width_ = 800;
  config.height_ = 600;
  scoped_ptr<MftH264Decoder> decoder(new MftH264Decoder(true));
  ASSERT_TRUE(decoder.get());
  decoder->Initialize(&loop, &handler, config);
  EXPECT_EQ(1, handler.init_count_);
  EXPECT_EQ(MftH264Decoder::kNormal, decoder->state());
  decoder->Uninitialize();
}

TEST_F(MftH264DecoderTest, DecoderUninit) {
  MessageLoop loop;
  SimpleMftH264DecoderHandler handler;
  VideoCodecConfig config;
  config.width_ = 800;
  config.height_ = 600;
  scoped_ptr<MftH264Decoder> decoder(new MftH264Decoder(false));
  ASSERT_TRUE(decoder.get());
  decoder->Initialize(&loop, &handler, config);
  EXPECT_EQ(MftH264Decoder::kNormal, decoder->state());
  decoder->Uninitialize();
  EXPECT_EQ(1, handler.uninit_count_);
  EXPECT_EQ(MftH264Decoder::kUninitialized, decoder->state());
}

TEST_F(MftH264DecoderTest, UninitBeforeInit) {
  MessageLoop loop;
  SimpleMftH264DecoderHandler handler;
  VideoCodecConfig config;
  config.width_ = 800;
  config.height_ = 600;
  scoped_ptr<MftH264Decoder> decoder(new MftH264Decoder(false));
  ASSERT_TRUE(decoder.get());
  decoder->Uninitialize();
  EXPECT_EQ(0, handler.uninit_count_);
}

TEST_F(MftH264DecoderTest, InitWithNegativeDimensions) {
  MessageLoop loop;
  SimpleMftH264DecoderHandler handler;
  VideoCodecConfig config;
  config.width_ = -123;
  config.height_ = -456;
  scoped_ptr<MftH264Decoder> decoder(new MftH264Decoder(false));
  ASSERT_TRUE(decoder.get());
  decoder->Initialize(&loop, &handler, config);
  EXPECT_EQ(MftH264Decoder::kNormal, decoder->state());
  EXPECT_EQ(kDecoderMaxWidth, handler.info_.stream_info_.surface_width_);
  EXPECT_EQ(kDecoderMaxHeight, handler.info_.stream_info_.surface_height_);
  decoder->Uninitialize();
}

TEST_F(MftH264DecoderTest, InitWithTooHighDimensions) {
  MessageLoop loop;
  SimpleMftH264DecoderHandler handler;
  VideoCodecConfig config;
  config.width_ = kDecoderMaxWidth + 1;
  config.height_ = kDecoderMaxHeight + 1;
  scoped_ptr<MftH264Decoder> decoder(new MftH264Decoder(false));
  ASSERT_TRUE(decoder.get());
  decoder->Initialize(&loop, &handler, config);
  EXPECT_EQ(MftH264Decoder::kNormal, decoder->state());
  EXPECT_EQ(kDecoderMaxWidth, handler.info_.stream_info_.surface_width_);
  EXPECT_EQ(kDecoderMaxHeight, handler.info_.stream_info_.surface_height_);
  decoder->Uninitialize();
}

TEST_F(MftH264DecoderTest, DrainOnEmptyBuffer) {
  MessageLoop loop;
  SimpleMftH264DecoderHandler handler;
  VideoCodecConfig config;
  config.width_ = 1024;
  config.height_ = 768;
  scoped_ptr<MftH264Decoder> decoder(new MftH264Decoder(false));
  ASSERT_TRUE(decoder.get());
  decoder->Initialize(&loop, &handler, config);
  EXPECT_EQ(MftH264Decoder::kNormal, decoder->state());
  scoped_refptr<Buffer> buffer(new DataBuffer(0));

  // Decoder should switch to drain mode because of this NULL buffer, and then
  // switch to kStopped when it says it needs more input during drain mode.
  decoder->EmptyThisBuffer(buffer);
  EXPECT_EQ(MftH264Decoder::kStopped, decoder->state());

  // Should have called back with one empty frame.
  EXPECT_EQ(1, handler.fill_buffer_callback_count_);
  ASSERT_TRUE(handler.current_frame_.get());
  EXPECT_EQ(VideoFrame::EMPTY, handler.current_frame_->format());
  decoder->Uninitialize();
}

TEST_F(MftH264DecoderTest, NoOutputOnGarbageInput) {
  // 100 samples of garbage.
  const int kNumFrames = 100;
  scoped_refptr<FakeMftReader> reader(new FakeMftReader(kNumFrames));
  ASSERT_TRUE(reader.get());

  MessageLoop loop;
  SimpleMftH264DecoderHandler handler;
  VideoCodecConfig config;
  config.width_ = 1024;
  config.height_ = 768;
  scoped_ptr<MftH264Decoder> decoder(new MftH264Decoder(false));
  ASSERT_TRUE(decoder.get());
  decoder->Initialize(&loop, &handler, config);
  EXPECT_EQ(MftH264Decoder::kNormal, decoder->state());
  handler.SetReader(reader);
  handler.SetDecoder(decoder.get());
  while (MftH264Decoder::kStopped != decoder->state()) {
    scoped_refptr<VideoFrame> frame;
    decoder->FillThisBuffer(frame);
  }

  // Output callback should only be invoked once - the empty frame to indicate
  // end of stream.
  EXPECT_EQ(1, handler.fill_buffer_callback_count_);
  ASSERT_TRUE(handler.current_frame_.get());
  EXPECT_EQ(VideoFrame::EMPTY, handler.current_frame_->format());

  // One extra count because of the end of stream NULL sample.
  EXPECT_EQ(kNumFrames, handler.empty_buffer_callback_count_ - 1);
  decoder->Uninitialize();
}

TEST_F(MftH264DecoderTest, FlushAtStart) {
  MessageLoop loop;
  SimpleMftH264DecoderHandler handler;
  VideoCodecConfig config;
  config.width_ = 1024;
  config.height_ = 768;
  scoped_ptr<MftH264Decoder> decoder(new MftH264Decoder(false));
  ASSERT_TRUE(decoder.get());
  decoder->Initialize(&loop, &handler, config);
  EXPECT_EQ(MftH264Decoder::kNormal, decoder->state());
  decoder->Flush();

  // Flush should succeed even if input/output are empty.
  EXPECT_EQ(1, handler.flush_count_);
  decoder->Uninitialize();
}

TEST_F(MftH264DecoderTest, NoFlushAtStopped) {
  scoped_refptr<BaseMftReader> reader(new FakeMftReader());
  ASSERT_TRUE(reader.get());

  MessageLoop loop;
  SimpleMftH264DecoderHandler handler;
  VideoCodecConfig config;
  config.width_ = 1024;
  config.height_ = 768;
  scoped_ptr<MftH264Decoder> decoder(new MftH264Decoder(false));
  ASSERT_TRUE(decoder.get());
  decoder->Initialize(&loop, &handler, config);
  EXPECT_EQ(MftH264Decoder::kNormal, decoder->state());
  handler.SetReader(reader);
  handler.SetDecoder(decoder.get());
  while (MftH264Decoder::kStopped != decoder->state()) {
    scoped_refptr<VideoFrame> frame;
    decoder->FillThisBuffer(frame);
  }
  EXPECT_EQ(0, handler.flush_count_);
  int old_flush_count = handler.flush_count_;
  decoder->Flush();
  EXPECT_EQ(old_flush_count, handler.flush_count_);
  decoder->Uninitialize();
}

FilePath GetVideoFilePath(const std::string& file_name) {
  FilePath path;
  PathService::Get(base::DIR_SOURCE_ROOT, &path);
  path = path.AppendASCII("media")
      .AppendASCII("test")
      .AppendASCII("data")
      .AppendASCII(file_name.c_str());
  return path;
}

void DecodeValidVideo(const std::string& filename, int num_frames, bool dxva) {
  scoped_refptr<FFmpegFileReaderWrapper> reader(new FFmpegFileReaderWrapper());
  ASSERT_TRUE(reader.get());
  FilePath path = GetVideoFilePath(filename);
  ASSERT_TRUE(file_util::PathExists(path));
  ASSERT_TRUE(reader->InitReader(WideToASCII(path.value())));
  int actual_width;
  int actual_height;
  ASSERT_TRUE(reader->GetWidth(&actual_width));
  ASSERT_TRUE(reader->GetHeight(&actual_height));

  MessageLoop loop;
  SimpleMftH264DecoderHandler handler;
  VideoCodecConfig config;
  config.width_ = 1;
  config.height_ = 1;
  scoped_ptr<MftH264Decoder> decoder(new MftH264Decoder(dxva));
  ASSERT_TRUE(decoder.get());
  decoder->Initialize(&loop, &handler, config);
  EXPECT_EQ(MftH264Decoder::kNormal, decoder->state());
  handler.SetReader(reader);
  handler.SetDecoder(decoder.get());
  while (MftH264Decoder::kStopped != decoder->state()) {
    scoped_refptr<VideoFrame> frame;
    decoder->FillThisBuffer(frame);
  }

  // We expect a format change when decoder receives enough data to determine
  // the actual frame width/height.
  EXPECT_GT(handler.format_change_count_, 0);
  EXPECT_EQ(actual_width, handler.info_.stream_info_.surface_width_);
  EXPECT_EQ(actual_height, handler.info_.stream_info_.surface_height_);
  EXPECT_GE(handler.empty_buffer_callback_count_, num_frames);
  EXPECT_EQ(num_frames, handler.fill_buffer_callback_count_ - 1);
  decoder->Uninitialize();
}

TEST_F(MftH264DecoderTest, DecodeValidVideoDxva) {
  DecodeValidVideo("bear.1280x720.mp4", 82, true);
}

TEST_F(MftH264DecoderTest, DecodeValidVideoNoDxva) {
  DecodeValidVideo("bear.1280x720.mp4", 82, false);
}

}  // namespace media
