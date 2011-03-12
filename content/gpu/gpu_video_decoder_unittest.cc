// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/process.h"
#include "content/common/gpu_messages.h"
#include "content/gpu/gpu_video_decoder.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_mock.h"
#include "ipc/ipc_message_utils.h"
#include "media/base/pipeline.h"
#include "media/video/video_mock_objects.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::DoAll;
using testing::NotNull;
using testing::Return;
using testing::SetArgumentPointee;

static const int32 kFrameId = 10;
static const int32 kDecoderHostId = 50;
static const media::VideoFrame::GlTexture kClientTexture = 101;
static const media::VideoFrame::GlTexture kServiceTexture = 102;
static const size_t kWidth = 320;
static const size_t kHeight = 240;

class MockGpuVideoDevice : public GpuVideoDevice {
 public:
  MockGpuVideoDevice() {}
  virtual ~MockGpuVideoDevice() {}

  MOCK_METHOD0(GetDevice, void*());
  MOCK_METHOD5(CreateVideoFrameFromGlTextures,
               bool(size_t, size_t, media::VideoFrame::Format,
                    const std::vector<media::VideoFrame::GlTexture>&,
                    scoped_refptr<media::VideoFrame>*));
  MOCK_METHOD1(ReleaseVideoFrame,
               void(const scoped_refptr<media::VideoFrame>& frame));
  MOCK_METHOD2(ConvertToVideoFrame,
               bool(void* buffer, scoped_refptr<media::VideoFrame> frame));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockGpuVideoDevice);
};

ACTION_P(InitializationDone, handler) {
  media::VideoCodecInfo info;
  info.success = true;
  info.provides_buffers = false;
  info.stream_info.surface_format = media::VideoFrame::RGBA;
  info.stream_info.surface_type = media::VideoFrame::TYPE_SYSTEM_MEMORY;
  info.stream_info.surface_width = kWidth;
  info.stream_info.surface_height = kHeight;
  handler->OnInitializeComplete(info);
}

ACTION_P(SendVideoFrameAllocated, handler) {
  std::vector<media::VideoFrame::GlTexture> textures;
  textures.push_back(kClientTexture);
  GpuVideoDecoderMsg_VideoFrameAllocated msg(0, kFrameId, textures);
  handler->OnMessageReceived(msg);
}

ACTION_P2(SendConsumeVideoFrame, handler, frame) {
  media::PipelineStatistics statistics;
  handler->ConsumeVideoFrame(frame, statistics);
}

class GpuVideoDecoderTest : public testing::Test,
                            public IPC::Message::Sender {
 public:
  GpuVideoDecoderTest() {
    // Create the mock objects.
    gles2_decoder_.reset(new gpu::gles2::MockGLES2Decoder());

    gpu_video_decoder_ = new GpuVideoDecoder(
        &message_loop_, kDecoderHostId, this, base::kNullProcessHandle,
        gles2_decoder_.get());

    // Create the mock objects.
    mock_engine_ = new media::MockVideoDecodeEngine();
    mock_device_ = new MockGpuVideoDevice();

    // Inject the mock objects.
    gpu_video_decoder_->SetVideoDecodeEngine(mock_engine_);
    gpu_video_decoder_->SetGpuVideoDevice(mock_device_);

    // VideoFrame for GpuVideoDevice.
    media::VideoFrame::GlTexture textures[] = { kServiceTexture, 0, 0 };
    media::VideoFrame::CreateFrameGlTexture(media::VideoFrame::RGBA,
                                            kWidth, kHeight, textures,
                                            &device_frame_);
  }

  virtual ~GpuVideoDecoderTest() {
     gpu_video_decoder_->SetVideoDecodeEngine(NULL);
     gpu_video_decoder_->SetGpuVideoDevice(NULL);
  }

  // This method is used to dispatch IPC messages to mock methods.
  virtual bool Send(IPC::Message* msg) {
    EXPECT_TRUE(msg);
    if (!msg)
      return false;

    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(GpuVideoDecoderTest, *msg)
      IPC_MESSAGE_HANDLER(GpuVideoDecoderHostMsg_InitializeACK,
                          OnInitializeDone)
      IPC_MESSAGE_HANDLER(GpuVideoDecoderHostMsg_DestroyACK,
                          OnUninitializeDone)
      IPC_MESSAGE_HANDLER(GpuVideoDecoderHostMsg_FlushACK,
                          OnFlushDone)
      IPC_MESSAGE_HANDLER(GpuVideoDecoderHostMsg_EmptyThisBufferACK,
                          OnEmptyThisBufferACK)
      IPC_MESSAGE_HANDLER(GpuVideoDecoderHostMsg_EmptyThisBufferDone,
                          OnEmptyThisBufferDone)
      IPC_MESSAGE_HANDLER(GpuVideoDecoderHostMsg_AllocateVideoFrames,
                          OnAllocateVideoFrames)
      IPC_MESSAGE_HANDLER(GpuVideoDecoderHostMsg_ReleaseAllVideoFrames,
                          OnReleaseAllVideoFrames)
      IPC_MESSAGE_HANDLER(GpuVideoDecoderHostMsg_ConsumeVideoFrame,
                          OnConsumeVideoFrame)
      IPC_MESSAGE_UNHANDLED_ERROR()
    IPC_END_MESSAGE_MAP()
    EXPECT_TRUE(handled);
    delete msg;
    return true;
  }

  // Mock methods for handling output IPC messages.
  MOCK_METHOD1(OnInitializeDone,
               void(const GpuVideoDecoderInitDoneParam& param));
  MOCK_METHOD0(OnUninitializeDone, void());
  MOCK_METHOD0(OnFlushDone, void());
  MOCK_METHOD0(OnEmptyThisBufferDone, void());
  MOCK_METHOD4(OnConsumeVideoFrame, void(int32 device_frame_id, int64 timestamp,
                                         int64 duration, int32 flags));
  MOCK_METHOD0(OnEmptyThisBufferACK, void());
  MOCK_METHOD4(OnAllocateVideoFrames, void(int32 n, uint32 width,
                                           uint32 height, int32 format));
  MOCK_METHOD0(OnReleaseAllVideoFrames, void());

  // Receive events from GpuVideoDecoder.
  MOCK_METHOD0(VideoFramesAllocated, void());

  void Initialize() {
    // VideoDecodeEngine is called.
    EXPECT_CALL(*mock_engine_, Initialize(_, _, _, _))
        .WillOnce(InitializationDone(gpu_video_decoder_));

    // Expect that initialization is completed.
    EXPECT_CALL(*this, OnInitializeDone(_));

    // Send an initialiaze message to GpuVideoDecoder.
    GpuVideoDecoderInitParam param;
    param.width = kWidth;
    param.height = kHeight;

    GpuVideoDecoderMsg_Initialize msg(0, param);
    gpu_video_decoder_->OnMessageReceived(msg);
  }

  void AllocateVideoFrames() {
    // Expect that IPC messages are sent. We'll reply with some GL textures.
    EXPECT_CALL(*this, OnAllocateVideoFrames(
        1, kWidth, kHeight, static_cast<int32>(media::VideoFrame::RGBA)))
        .WillOnce(SendVideoFrameAllocated(gpu_video_decoder_));

    // Expect that MakeCurrent() is called.
    EXPECT_CALL(*gles2_decoder_.get(), MakeCurrent())
        .WillOnce(Return(true))
        .RetiresOnSaturation();

    // Expect that translate method is called.
    EXPECT_CALL(*gles2_decoder_.get(),
                GetServiceTextureId(kClientTexture, NotNull()))
        .WillOnce(DoAll(SetArgumentPointee<1>(kServiceTexture), Return(true)));

    // And then GpuVideoDevice is called to create VideoFrame from GL textures.
    EXPECT_CALL(*mock_device_,
                CreateVideoFrameFromGlTextures(kWidth, kHeight,
                                               media::VideoFrame::RGBA, _,
                                               NotNull()))
        .WillOnce(DoAll(SetArgumentPointee<4>(device_frame_), Return(true)));

    // Finally the task is called.
    EXPECT_CALL(*this, VideoFramesAllocated());

    // Pretend calling GpuVideoDecoder for allocating frames.
    gpu_video_decoder_->AllocateVideoFrames(
        1, kWidth, kHeight, media::VideoFrame::RGBA, &decoder_frames_,
        NewRunnableMethod(this, &GpuVideoDecoderTest::VideoFramesAllocated));
  }

  void ReleaseVideoFrames() {
    // Expect that MakeCurrent() is called.
    EXPECT_CALL(*gles2_decoder_.get(), MakeCurrent())
        .WillOnce(Return(true))
        .RetiresOnSaturation();

    // Expect that video frame is released.
    EXPECT_CALL(*mock_device_, ReleaseVideoFrame(device_frame_));

    // Expect that IPC message is send to release video frame.
    EXPECT_CALL(*this, OnReleaseAllVideoFrames());

    // Call to GpuVideoDecoder to release all video frames.
    gpu_video_decoder_->ReleaseAllVideoFrames();
  }

  void BufferExchange() {
    // Expect that we call to produce video frame.
    EXPECT_CALL(*mock_engine_, ProduceVideoFrame(device_frame_))
        .WillOnce(SendConsumeVideoFrame(gpu_video_decoder_, device_frame_))
        .RetiresOnSaturation();

    // Expect that consume video frame is called.
    EXPECT_CALL(*this, OnConsumeVideoFrame(kFrameId, 0, 0, 0))
        .RetiresOnSaturation();

    // Ask the GpuVideoDecoder to produce a video frame.
    GpuVideoDecoderMsg_ProduceVideoFrame msg(0, kFrameId);
    gpu_video_decoder_->OnMessageReceived(msg);
  }

 private:
  scoped_refptr<GpuVideoDecoder> gpu_video_decoder_;
  MockGpuVideoDevice* mock_device_;
  media::MockVideoDecodeEngine* mock_engine_;
  scoped_ptr<gpu::gles2::MockGLES2Decoder> gles2_decoder_;
  std::vector<scoped_refptr<media::VideoFrame> > decoder_frames_;
  scoped_refptr<media::VideoFrame> device_frame_;

  MessageLoop message_loop_;

  DISALLOW_COPY_AND_ASSIGN(GpuVideoDecoderTest);
};

TEST_F(GpuVideoDecoderTest, Initialize) {
  Initialize();
}

TEST_F(GpuVideoDecoderTest, AllocateVideoFrames) {
  Initialize();
  AllocateVideoFrames();
}

TEST_F(GpuVideoDecoderTest, ReleaseVideoFrames) {
  Initialize();
  AllocateVideoFrames();
  ReleaseVideoFrames();
}

TEST_F(GpuVideoDecoderTest, BufferExchange) {
  Initialize();
  AllocateVideoFrames();
  BufferExchange();
  BufferExchange();
  ReleaseVideoFrames();
}

DISABLE_RUNNABLE_METHOD_REFCOUNT(GpuVideoDecoderTest);
