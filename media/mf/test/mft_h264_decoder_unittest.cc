// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <d3d9.h>
#include <dxva2api.h>
#include <mfapi.h>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/scoped_comptr_win.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "media/base/data_buffer.h"
#include "media/base/video_frame.h"
#include "media/mf/d3d_util.h"
#include "media/mf/file_reader_util.h"
#include "media/mf/mft_h264_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

static const int kDecoderMaxWidth = 1920;
static const int kDecoderMaxHeight = 1088;

class FakeMftReader {
 public:
  FakeMftReader() : frames_remaining_(20) {}
  explicit FakeMftReader(int count) : frames_remaining_(count) {}
  ~FakeMftReader() {}

  // Provides garbage input to the decoder.
  void ReadCallback(scoped_refptr<DataBuffer>* input) {
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

class FakeMftRenderer : public base::RefCountedThreadSafe<FakeMftRenderer> {
 public:
  explicit FakeMftRenderer(scoped_refptr<MftH264Decoder> decoder)
      : decoder_(decoder),
        count_(0),
        flush_countdown_(0) {
  }

  virtual ~FakeMftRenderer() {}

  virtual void WriteCallback(scoped_refptr<VideoFrame> frame) {
    static_cast<IMFMediaBuffer*>(frame->private_buffer())->Release();
    ++count_;
    if (flush_countdown_ > 0) {
      if (--flush_countdown_ == 0) {
        decoder_->Flush();
      }
    }
    MessageLoop::current()->PostTask(
        FROM_HERE,
        NewRunnableMethod(decoder_.get(), &MftH264Decoder::GetOutput));
  }

  virtual void Start() {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        NewRunnableMethod(decoder_.get(), &MftH264Decoder::GetOutput));
  }

  virtual void OnDecodeError(MftH264Decoder::Error error) {
    MessageLoop::current()->Quit();
  }

  virtual void SetFlushCountdown(int countdown) {
    flush_countdown_ = countdown;
  }

  int count() const { return count_; }

 protected:
  scoped_refptr<MftH264Decoder> decoder_;
  int count_;
  int flush_countdown_;
};

class MftH264DecoderTest : public testing::Test {
 public:
  MftH264DecoderTest() {}
  virtual ~MftH264DecoderTest() {}

 protected:
  virtual void SetUp() {}
  virtual void TearDown() {}
};

// A simple test case for init/deinit of MF/COM libraries.
TEST_F(MftH264DecoderTest, SimpleInit) {
  EXPECT_HRESULT_SUCCEEDED(
      CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE));
  EXPECT_HRESULT_SUCCEEDED(MFStartup(MF_VERSION, MFSTARTUP_FULL));
  EXPECT_HRESULT_SUCCEEDED(MFShutdown());
  CoUninitialize();
}

TEST_F(MftH264DecoderTest, InitWithDxvaButNoD3dDevice) {
  scoped_refptr<MftH264Decoder> decoder(new MftH264Decoder(true));
  ASSERT_TRUE(decoder.get() != NULL);
  FakeMftReader reader;
  scoped_refptr<FakeMftRenderer> renderer(new FakeMftRenderer(decoder));
  EXPECT_FALSE(
      decoder->Init(NULL, 6, 7, 111, 222, 3, 1,
                    NewCallback(&reader, &FakeMftReader::ReadCallback),
                    NewCallback(renderer.get(),
                                &FakeMftRenderer::WriteCallback),
                    NewCallback(renderer.get(),
                                &FakeMftRenderer::OnDecodeError)));
}

TEST_F(MftH264DecoderTest, InitMissingCallbacks) {
  scoped_refptr<MftH264Decoder> decoder(new MftH264Decoder(false));
  ASSERT_TRUE(decoder.get() != NULL);
  EXPECT_FALSE(decoder->Init(NULL, 1, 3, 111, 222, 56, 34, NULL, NULL, NULL));
}

TEST_F(MftH264DecoderTest, InitWithNegativeDimensions) {
  scoped_refptr<MftH264Decoder> decoder(new MftH264Decoder(false));
  ASSERT_TRUE(decoder.get() != NULL);
  FakeMftReader reader;
  scoped_refptr<FakeMftRenderer> renderer(new FakeMftRenderer(decoder));
  EXPECT_TRUE(decoder->Init(NULL, 0, 6, -123, -456, 22, 4787,
                            NewCallback(&reader, &FakeMftReader::ReadCallback),
                            NewCallback(renderer.get(),
                                        &FakeMftRenderer::WriteCallback),
                            NewCallback(renderer.get(),
                                        &FakeMftRenderer::OnDecodeError)));

  // By default, decoder should "guess" the dimensions to be the maximum.
  EXPECT_EQ(kDecoderMaxWidth, decoder->width());
  EXPECT_EQ(kDecoderMaxHeight, decoder->height());
}

TEST_F(MftH264DecoderTest, InitWithTooHighDimensions) {
  scoped_refptr<MftH264Decoder> decoder(new MftH264Decoder(false));
  ASSERT_TRUE(decoder.get() != NULL);
  FakeMftReader reader;
  scoped_refptr<FakeMftRenderer> renderer(new FakeMftRenderer(decoder));
  EXPECT_TRUE(decoder->Init(NULL, 0, 0,
                            kDecoderMaxWidth + 1, kDecoderMaxHeight + 1,
                            0, 0,
                            NewCallback(&reader, &FakeMftReader::ReadCallback),
                            NewCallback(renderer.get(),
                                        &FakeMftRenderer::WriteCallback),
                            NewCallback(renderer.get(),
                                        &FakeMftRenderer::OnDecodeError)));

  // Decoder should truncate the dimensions to the maximum supported.
  EXPECT_EQ(kDecoderMaxWidth, decoder->width());
  EXPECT_EQ(kDecoderMaxHeight, decoder->height());
}

TEST_F(MftH264DecoderTest, InitWithNormalDimensions) {
  scoped_refptr<MftH264Decoder> decoder(new MftH264Decoder(false));
  ASSERT_TRUE(decoder.get() != NULL);
  FakeMftReader reader;
  scoped_refptr<FakeMftRenderer> renderer(new FakeMftRenderer(decoder));
  int width = 1024, height = 768;
  EXPECT_TRUE(decoder->Init(NULL, 0, 0, width, height, 0, 0,
                            NewCallback(&reader, &FakeMftReader::ReadCallback),
                            NewCallback(renderer.get(),
                                        &FakeMftRenderer::WriteCallback),
                            NewCallback(renderer.get(),
                                        &FakeMftRenderer::OnDecodeError)));

  EXPECT_EQ(width, decoder->width());
  EXPECT_EQ(height, decoder->height());
}

// SendDrainMessage() is not a public method. Nonetheless it does not hurt
// to test that the decoder should not do things before it is initialized.
TEST_F(MftH264DecoderTest, SendDrainMessageBeforeInitDeathTest) {
  scoped_refptr<MftH264Decoder> decoder(new MftH264Decoder(false));
  ASSERT_TRUE(decoder.get() != NULL);
  EXPECT_DEATH({ decoder->SendDrainMessage(); }, ".*initialized_.*");
}

// Tests draining after init, but before any input is sent.
TEST_F(MftH264DecoderTest, SendDrainMessageAtInit) {
  scoped_refptr<MftH264Decoder> decoder(new MftH264Decoder(false));
  ASSERT_TRUE(decoder.get() != NULL);
  FakeMftReader reader;
  scoped_refptr<FakeMftRenderer> renderer(new FakeMftRenderer(decoder));
  ASSERT_TRUE(decoder->Init(NULL, 0, 0, 111, 222, 0, 0,
                            NewCallback(&reader, &FakeMftReader::ReadCallback),
                            NewCallback(renderer.get(),
                                        &FakeMftRenderer::WriteCallback),
                            NewCallback(renderer.get(),
                                        &FakeMftRenderer::OnDecodeError)));
  EXPECT_TRUE(decoder->SendDrainMessage());
  EXPECT_TRUE(decoder->drain_message_sent_);
}

TEST_F(MftH264DecoderTest, DrainOnEndOfInputStream) {
  MessageLoop loop;
  scoped_refptr<MftH264Decoder> decoder(new MftH264Decoder(false));
  ASSERT_TRUE(decoder.get() != NULL);

  // No frames, outputs a NULL indicating end-of-stream
  FakeMftReader reader(0);
  scoped_refptr<FakeMftRenderer> renderer(new FakeMftRenderer(decoder));
  ASSERT_TRUE(decoder->Init(NULL, 0, 0, 111, 222, 0, 0,
                            NewCallback(&reader, &FakeMftReader::ReadCallback),
                            NewCallback(renderer.get(),
                                        &FakeMftRenderer::WriteCallback),
                            NewCallback(renderer.get(),
                                        &FakeMftRenderer::OnDecodeError)));
  MessageLoop::current()->PostTask(
      FROM_HERE,
      NewRunnableMethod(renderer.get(), &FakeMftRenderer::Start));
  MessageLoop::current()->Run();
  EXPECT_TRUE(decoder->drain_message_sent());
}

// 100 input garbage samples should be enough to test whether the decoder
// will output decoded garbage frames.
TEST_F(MftH264DecoderTest, NoOutputOnGarbageInput) {
  MessageLoop loop;
  scoped_refptr<MftH264Decoder> decoder(new MftH264Decoder(false));
  ASSERT_TRUE(decoder.get() != NULL);
  int num_frames = 100;
  FakeMftReader reader(num_frames);
  scoped_refptr<FakeMftRenderer> renderer(new FakeMftRenderer(decoder));
  ASSERT_TRUE(decoder->Init(NULL, 0, 0, 111, 222, 0, 0,
                            NewCallback(&reader, &FakeMftReader::ReadCallback),
                            NewCallback(renderer.get(),
                                        &FakeMftRenderer::WriteCallback),
                            NewCallback(renderer.get(),
                                        &FakeMftRenderer::OnDecodeError)));
  MessageLoop::current()->PostTask(
      FROM_HERE, NewRunnableMethod(renderer.get(), &FakeMftRenderer::Start));
  MessageLoop::current()->Run();

  // Decoder should accept corrupt input data and silently ignore it.
  EXPECT_EQ(num_frames, decoder->frames_read());

  // Decoder should not have output anything if input is corrupt.
  EXPECT_EQ(0, decoder->frames_decoded());
  EXPECT_EQ(0, renderer->count());
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

// Decodes media/test/data/bear.1280x720.mp4 which is expected to be a valid
// H.264 video.
TEST_F(MftH264DecoderTest, DecodeValidVideoDxva) {
  MessageLoop loop;
  FilePath path = GetVideoFilePath("bear.1280x720.mp4");
  ASSERT_TRUE(file_util::PathExists(path));

  ScopedComPtr<IDirect3D9> d3d9;
  ScopedComPtr<IDirect3DDevice9> device;
  ScopedComPtr<IDirect3DDeviceManager9> dev_manager;
  dev_manager.Attach(CreateD3DDevManager(GetDesktopWindow(),
                                         d3d9.Receive(),
                                         device.Receive()));
  ASSERT_TRUE(dev_manager.get() != NULL);

  scoped_refptr<MftH264Decoder> decoder(new MftH264Decoder(true));
  ASSERT_TRUE(decoder.get() != NULL);
  scoped_ptr<FFmpegFileReader> reader(
      new FFmpegFileReader(WideToASCII(path.value())));
  ASSERT_TRUE(reader->Initialize());
  scoped_refptr<FakeMftRenderer> renderer(new FakeMftRenderer(decoder));
  ASSERT_TRUE(decoder->Init(dev_manager.get(), 0, 0, 111, 222, 0, 0,
                            NewCallback(reader.get(), &FFmpegFileReader::Read),
                            NewCallback(renderer.get(),
                                        &FakeMftRenderer::WriteCallback),
                            NewCallback(renderer.get(),
                                        &FakeMftRenderer::OnDecodeError)));
  MessageLoop::current()->PostTask(
      FROM_HERE,
      NewRunnableMethod(renderer.get(), &FakeMftRenderer::Start));
  MessageLoop::current()->Run();

  // If the video is valid, then it should output frames. However, for some
  // videos, the number of frames decoded is one-off.
  EXPECT_EQ(82, decoder->frames_read());
  EXPECT_LE(decoder->frames_read() - decoder->frames_decoded(), 1);
}

TEST_F(MftH264DecoderTest, FlushAtInit) {
  scoped_refptr<MftH264Decoder> decoder(new MftH264Decoder(false));
  ASSERT_TRUE(decoder.get() != NULL);
  FakeMftReader reader;
  scoped_refptr<FakeMftRenderer> renderer(new FakeMftRenderer(decoder));
  ASSERT_TRUE(decoder->Init(NULL, 0, 0, 111, 222, 0, 0,
                            NewCallback(&reader, &FakeMftReader::ReadCallback),
                            NewCallback(renderer.get(),
                                        &FakeMftRenderer::WriteCallback),
                            NewCallback(renderer.get(),
                                        &FakeMftRenderer::OnDecodeError)));
  EXPECT_TRUE(decoder->Flush());
}

TEST_F(MftH264DecoderTest, FlushAtEnd) {
  MessageLoop loop;
  scoped_refptr<MftH264Decoder> decoder(new MftH264Decoder(false));
  ASSERT_TRUE(decoder.get() != NULL);

  // No frames, outputs a NULL indicating end-of-stream
  FakeMftReader reader(0);
  scoped_refptr<FakeMftRenderer> renderer(new FakeMftRenderer(decoder));
  ASSERT_TRUE(decoder->Init(NULL, 0, 0, 111, 222, 0, 0,
                            NewCallback(&reader, &FakeMftReader::ReadCallback),
                            NewCallback(renderer.get(),
                                        &FakeMftRenderer::WriteCallback),
                            NewCallback(renderer.get(),
                                        &FakeMftRenderer::OnDecodeError)));
  MessageLoop::current()->PostTask(
      FROM_HERE,
      NewRunnableMethod(renderer.get(), &FakeMftRenderer::Start));
  MessageLoop::current()->Run();
  EXPECT_TRUE(decoder->Flush());
}

TEST_F(MftH264DecoderTest, FlushAtMiddle) {
  MessageLoop loop;
  FilePath path = GetVideoFilePath("bear.1280x720.mp4");
  ASSERT_TRUE(file_util::PathExists(path));

  ScopedComPtr<IDirect3D9> d3d9;
  ScopedComPtr<IDirect3DDevice9> device;
  ScopedComPtr<IDirect3DDeviceManager9> dev_manager;
  dev_manager.Attach(CreateD3DDevManager(GetDesktopWindow(),
                                         d3d9.Receive(),
                                         device.Receive()));
  ASSERT_TRUE(dev_manager.get() != NULL);

  scoped_refptr<MftH264Decoder> decoder(new MftH264Decoder(true));
  ASSERT_TRUE(decoder.get() != NULL);
  scoped_ptr<FFmpegFileReader> reader(
      new FFmpegFileReader(WideToASCII(path.value())));
  ASSERT_TRUE(reader->Initialize());
  scoped_refptr<FakeMftRenderer> renderer(new FakeMftRenderer(decoder));
  ASSERT_TRUE(renderer.get());

  // Flush after obtaining 40 decode frames. There are no more key frames after
  // the first one, so we expect it to stop outputting frames after flush.
  int flush_at_nth_decoded_frame = 40;
  renderer->SetFlushCountdown(flush_at_nth_decoded_frame);
  ASSERT_TRUE(decoder->Init(dev_manager.get(), 0, 0, 111, 222, 0, 0,
                            NewCallback(reader.get(), &FFmpegFileReader::Read),
                            NewCallback(renderer.get(),
                                        &FakeMftRenderer::WriteCallback),
                            NewCallback(renderer.get(),
                                        &FakeMftRenderer::OnDecodeError)));
  MessageLoop::current()->PostTask(
      FROM_HERE,
      NewRunnableMethod(renderer.get(), &FakeMftRenderer::Start));
  MessageLoop::current()->Run();
  EXPECT_EQ(82, decoder->frames_read());
  EXPECT_EQ(decoder->frames_decoded(), flush_at_nth_decoded_frame);
}

}  // namespace media
