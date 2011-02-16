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
#include "media/tools/mft_h264_example/file_reader_util.h"
#include "media/video/mft_h264_decode_engine.h"
#include "media/video/mft_h264_decode_engine_context.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;

namespace media {

static const int kDecoderMaxWidth = 1920;
static const int kDecoderMaxHeight = 1088;

// Helper classes

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

class SimpleMftH264DecodeEngineHandler
    : public VideoDecodeEngine::EventHandler {
 public:
  SimpleMftH264DecodeEngineHandler()
      : init_count_(0),
        uninit_count_(0),
        flush_count_(0),
        format_change_count_(0),
        empty_buffer_callback_count_(0),
        fill_buffer_callback_count_(0) {
    memset(&info_, 0, sizeof(info_));
  }
  virtual ~SimpleMftH264DecodeEngineHandler() {}
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
    info_.stream_info = stream_info;
  }
  virtual void ProduceVideoSample(scoped_refptr<Buffer> buffer) {
    if (reader_.get() && decoder_) {
      empty_buffer_callback_count_++;
      scoped_refptr<DataBuffer> input;
      reader_->ReadCallback(&input);
      decoder_->ConsumeVideoSample(input);
    }
  }
  virtual void ConsumeVideoFrame(scoped_refptr<VideoFrame> frame,
                                 const PipelineStatistics& statistics) {
    fill_buffer_callback_count_++;
    current_frame_ = frame;
  }
  void SetReader(scoped_refptr<BaseMftReader> reader) {
    reader_ = reader;
  }
  void SetDecodeEngine(MftH264DecodeEngine* decoder) {
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
  MftH264DecodeEngine* decoder_;
  scoped_refptr<VideoFrame> current_frame_;
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

// Helper functions

static FilePath GetVideoFilePath(const std::string& file_name) {
  FilePath path;
  PathService::Get(base::DIR_SOURCE_ROOT, &path);
  path = path.AppendASCII("media")
      .AppendASCII("test")
      .AppendASCII("data")
      .AppendASCII(file_name.c_str());
  return path;
}

class MftH264DecodeEngineTest : public testing::Test {
 protected:
  MftH264DecodeEngineTest()
      : loop_(),
        window_(NULL),
        handler_(NULL),
        engine_(NULL),
        context_(NULL) {
  }
  virtual ~MftH264DecodeEngineTest() {}
  virtual void SetUp() {
    handler_.reset(new SimpleMftH264DecodeEngineHandler());
  }
  virtual void TearDown() {
    if (context_.get()) {
      context_->ReleaseAllVideoFrames();
      context_->Destroy(NULL);
    }
    if (window_)
      DestroyWindow(window_);
  }
  void GetDecodeEngine(bool dxva) {
    if (dxva) {
      if (!window_)
        CreateDrawWindow();
      context_.reset(new MftH264DecodeEngineContext(window_));
      ASSERT_TRUE(context_.get());
      context_->Initialize(NULL);
      ASSERT_TRUE(context_->initialized());
    }
    engine_.reset(new MftH264DecodeEngine(dxva));
    ASSERT_TRUE(engine_.get());
  }
  void InitDecodeEngine(int width, int height) {
    VideoCodecConfig config;
    config.width = width;
    config.height = height;

    // Note that although |config| is passed as reference, |config| is copied
    // into the decode engine, so it is okay to make |config| a local variable.
    engine_->Initialize(&loop_, handler_.get(), context_.get(), config);
    EXPECT_EQ(1, handler_->init_count_);
    EXPECT_EQ(MftH264DecodeEngine::kNormal, engine_->state());
  }
  void InitDecodeEngine() {
    InitDecodeEngine(800, 600);
  }
  void TestInitAndUninit(bool dxva) {
    GetDecodeEngine(dxva);
    InitDecodeEngine();
    engine_->Uninitialize();
  }
  void DecodeAll(scoped_refptr<BaseMftReader> reader) {
    handler_->SetReader(reader);
    handler_->SetDecodeEngine(engine_.get());
    while (MftH264DecodeEngine::kStopped != engine_->state()) {
      scoped_refptr<VideoFrame> frame;
      engine_->ProduceVideoFrame(frame);
    }
  }
  void DecodeValidVideo(const std::string& filename, int num_frames,
                        bool dxva) {
    scoped_refptr<FFmpegFileReaderWrapper> reader(
        new FFmpegFileReaderWrapper());
    ASSERT_TRUE(reader.get());
    FilePath path = GetVideoFilePath(filename);
    ASSERT_TRUE(file_util::PathExists(path));
    ASSERT_TRUE(reader->InitReader(WideToASCII(path.value())));
    int actual_width;
    int actual_height;
    ASSERT_TRUE(reader->GetWidth(&actual_width));
    ASSERT_TRUE(reader->GetHeight(&actual_height));

    VideoCodecConfig config;
    CreateDrawWindow(config.width, config.height);
    GetDecodeEngine(dxva);
    InitDecodeEngine();
    DecodeAll(reader);

    // We expect a format change when decoder receives enough data to determine
    // the actual frame width/height.
    EXPECT_GT(handler_->format_change_count_, 0);
    EXPECT_EQ(actual_width, handler_->info_.stream_info.surface_width);
    EXPECT_EQ(actual_height, handler_->info_.stream_info.surface_height);
    EXPECT_GE(handler_->empty_buffer_callback_count_, num_frames);
    EXPECT_EQ(num_frames, handler_->fill_buffer_callback_count_ - 1);
    engine_->Uninitialize();
  }
  void ExpectDefaultDimensionsOnInput(int width, int height) {
    GetDecodeEngine(false);
    InitDecodeEngine(width, height);
    EXPECT_EQ(kDecoderMaxWidth, handler_->info_.stream_info.surface_width);
    EXPECT_EQ(kDecoderMaxHeight, handler_->info_.stream_info.surface_height);
    engine_->Uninitialize();
  }

  scoped_ptr<SimpleMftH264DecodeEngineHandler> handler_;
  scoped_ptr<MftH264DecodeEngine> engine_;
  scoped_ptr<MftH264DecodeEngineContext> context_;

 private:
  void CreateDrawWindow(int width, int height) {
    static const wchar_t kClassName[] = L"Test";
    static const wchar_t kWindowTitle[] = L"MFT Unittest Draw Window";
    WNDCLASS window_class = {0};
    window_class.lpszClassName = kClassName;
    window_class.hInstance = NULL;
    window_class.hbrBackground = 0;
    window_class.lpfnWndProc = DefWindowProc;
    window_class.hCursor = 0;
    RegisterClass(&window_class);
    window_ = CreateWindow(kClassName,
                           kWindowTitle,
                           (WS_OVERLAPPEDWINDOW | WS_VISIBLE) &
                            ~(WS_MAXIMIZEBOX | WS_THICKFRAME),
                           100, 100, width, height,
                           NULL, NULL, NULL, NULL);
    ASSERT_TRUE(window_);
  }
  void CreateDrawWindow() {
    CreateDrawWindow(800, 600);
  }

  MessageLoop loop_;
  HWND window_;
};

// A simple test case for init/deinit of MF/COM libraries.
TEST_F(MftH264DecodeEngineTest, LibraryInit) {
  EXPECT_TRUE(MftH264DecodeEngine::StartupComLibraries());
  MftH264DecodeEngine::ShutdownComLibraries();
}

TEST_F(MftH264DecodeEngineTest, DecoderUninitializedAtFirst) {
  GetDecodeEngine(true);
  EXPECT_EQ(MftH264DecodeEngine::kUninitialized, engine_->state());
}

TEST_F(MftH264DecodeEngineTest, DecoderInitMissingArgs) {
  VideoCodecConfig config;
  GetDecodeEngine(false);
  engine_->Initialize(NULL, NULL, NULL, config);
  EXPECT_EQ(MftH264DecodeEngine::kUninitialized, engine_->state());
}

TEST_F(MftH264DecodeEngineTest, DecoderInitNoDxva) {
  TestInitAndUninit(false);
}

TEST_F(MftH264DecodeEngineTest, DecoderInitDxva) {
  TestInitAndUninit(true);
}

TEST_F(MftH264DecodeEngineTest, DecoderUninit) {
  TestInitAndUninit(false);
  EXPECT_EQ(1, handler_->uninit_count_);
  EXPECT_EQ(MftH264DecodeEngine::kUninitialized, engine_->state());
}

TEST_F(MftH264DecodeEngineTest, UninitBeforeInit) {
  GetDecodeEngine(false);
  engine_->Uninitialize();
  EXPECT_EQ(0, handler_->uninit_count_);
}

TEST_F(MftH264DecodeEngineTest, InitWithNegativeDimensions) {
  ExpectDefaultDimensionsOnInput(-123, -456);
}

TEST_F(MftH264DecodeEngineTest, InitWithTooHighDimensions) {
  ExpectDefaultDimensionsOnInput(kDecoderMaxWidth + 1, kDecoderMaxHeight + 1);
}

TEST_F(MftH264DecodeEngineTest, DrainOnEmptyBuffer) {
  GetDecodeEngine(false);
  InitDecodeEngine();

  // Decoder should switch to drain mode because of this NULL buffer, and then
  // switch to kStopped when it says it needs more input during drain mode.
  scoped_refptr<Buffer> buffer(new DataBuffer(0));
  engine_->ConsumeVideoSample(buffer);
  EXPECT_EQ(MftH264DecodeEngine::kStopped, engine_->state());

  // Should have called back with one empty frame.
  EXPECT_EQ(1, handler_->fill_buffer_callback_count_);
  ASSERT_TRUE(handler_->current_frame_.get());
  EXPECT_EQ(VideoFrame::EMPTY, handler_->current_frame_->format());
  engine_->Uninitialize();
}

TEST_F(MftH264DecodeEngineTest, NoOutputOnGarbageInput) {
  // 100 samples of garbage.
  const int kNumFrames = 100;
  scoped_refptr<FakeMftReader> reader(new FakeMftReader(kNumFrames));
  ASSERT_TRUE(reader.get());

  GetDecodeEngine(false);
  InitDecodeEngine();
  DecodeAll(reader);

  // Output callback should only be invoked once - the empty frame to indicate
  // end of stream.
  EXPECT_EQ(1, handler_->fill_buffer_callback_count_);
  ASSERT_TRUE(handler_->current_frame_.get());
  EXPECT_EQ(VideoFrame::EMPTY, handler_->current_frame_->format());

  // One extra count because of the end of stream NULL sample.
  EXPECT_EQ(kNumFrames, handler_->empty_buffer_callback_count_ - 1);
  engine_->Uninitialize();
}

TEST_F(MftH264DecodeEngineTest, FlushAtStart) {
  GetDecodeEngine(false);
  InitDecodeEngine();
  engine_->Flush();

  // Flush should succeed even if input/output are empty.
  EXPECT_EQ(1, handler_->flush_count_);
  engine_->Uninitialize();
}

TEST_F(MftH264DecodeEngineTest, NoFlushAtStopped) {
  scoped_refptr<BaseMftReader> reader(new FakeMftReader());
  ASSERT_TRUE(reader.get());

  GetDecodeEngine(false);
  InitDecodeEngine();
  DecodeAll(reader);

  EXPECT_EQ(0, handler_->flush_count_);
  int old_flush_count = handler_->flush_count_;
  engine_->Flush();
  EXPECT_EQ(old_flush_count, handler_->flush_count_);
  engine_->Uninitialize();
}

TEST_F(MftH264DecodeEngineTest, DecodeValidVideoDxva) {
  DecodeValidVideo("bear.1280x720.mp4", 82, true);
}

TEST_F(MftH264DecodeEngineTest, DecodeValidVideoNoDxva) {
  DecodeValidVideo("bear.1280x720.mp4", 82, false);
}

}  // namespace media
