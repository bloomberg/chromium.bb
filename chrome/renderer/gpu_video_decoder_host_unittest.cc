// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "chrome/common/gpu_messages.h"
#include "chrome/common/message_router.h"
#include "chrome/renderer/gpu_video_decoder_host.h"
#include "media/video/mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::DoAll;
using testing::NotNull;
using testing::Return;
using testing::SetArgumentPointee;

static const int kContextRouteId = 50;
static const int kDecoderHostId = 51;
static const int kDecoderId = 51;
static const int kVideoFrames = 3;
static const int kWidth = 320;
static const int kHeight = 240;
static const int kTransportBufferSize = 1024;

ACTION_P(SimulateAllocateVideoFrames, frames) {
  // Fake some texture IDs here.
  media::VideoFrame::GlTexture textures[] = {4, 5, 6};
  for (int i = 0; i < kVideoFrames; ++i) {
    scoped_refptr<media::VideoFrame> frame;
    media::VideoFrame::CreateFrameGlTexture(media::VideoFrame::YV12,
                                            kWidth, kHeight, textures,
                                            &frame);
    frames->push_back(frame);
    arg4->push_back(frame);
  }

  // Execute the callback to complete the task.
  arg5->Run();
  delete arg5;
}

ACTION_P2(SendMessage, handler, msg) {
  handler->OnMessageReceived(msg);
}

class GpuVideoDecoderHostTest : public testing::Test,
                                public IPC::Message::Sender,
                                public media::VideoDecodeEngine::EventHandler {
 public:
  // This method is used to dispatch IPC messages to mock methods.
  virtual bool Send(IPC::Message* msg) {
    EXPECT_TRUE(msg);
    if (!msg)
      return false;

    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(GpuVideoDecoderHostTest, *msg)
      IPC_MESSAGE_HANDLER(GpuChannelMsg_CreateVideoDecoder,
                          OnCreateVideoDecoder)
      IPC_MESSAGE_HANDLER(GpuVideoDecoderMsg_Initialize,
                          OnInitialize)
      IPC_MESSAGE_HANDLER(GpuVideoDecoderMsg_Destroy,
                          OnDestroy)
      IPC_MESSAGE_HANDLER(GpuVideoDecoderMsg_Flush,
                          OnFlush)
      IPC_MESSAGE_HANDLER(GpuVideoDecoderMsg_EmptyThisBuffer,
                          OnEmptyThisBuffer)
      IPC_MESSAGE_HANDLER(GpuVideoDecoderMsg_VideoFrameAllocated,
                          OnVideoFrameAllocated)
      IPC_MESSAGE_HANDLER(GpuVideoDecoderMsg_ProduceVideoFrame,
                          OnProduceVideoFrame)
      IPC_MESSAGE_UNHANDLED_ERROR()
    IPC_END_MESSAGE_MAP()
    EXPECT_TRUE(handled);
    delete msg;
    return true;
  }

  // Mock methods for outgoing messages.
  MOCK_METHOD1(OnInitialize, void(GpuVideoDecoderInitParam param));
  MOCK_METHOD0(OnDestroy, void());
  MOCK_METHOD0(OnFlush, void());
  MOCK_METHOD1(OnEmptyThisBuffer,
               void(GpuVideoDecoderInputBufferParam param));
  MOCK_METHOD1(OnProduceVideoFrame, void(int32 frame_id));
  MOCK_METHOD2(OnVideoFrameAllocated,
               void(int32 frame_id, std::vector<uint32> textures));
  MOCK_METHOD2(OnCreateVideoDecoder,
               void(int32 context_route_id, int32 decoder_host_id));

  // Mock methods for VideoDecodeEngine::EventHandler.
  MOCK_METHOD1(ProduceVideoSample,
               void(scoped_refptr<media::Buffer> buffer));
  MOCK_METHOD1(ConsumeVideoFrame,
               void(scoped_refptr<media::VideoFrame> frame));
  MOCK_METHOD1(OnInitializeComplete,
               void(const media::VideoCodecInfo& info));
  MOCK_METHOD0(OnUninitializeComplete, void());
  MOCK_METHOD0(OnFlushComplete, void());
  MOCK_METHOD0(OnSeekComplete, void());
  MOCK_METHOD0(OnError, void());
  MOCK_METHOD1(OnFormatChange,
               void(media::VideoStreamInfo stream_info));

  void Initialize() {
    decoder_host_.reset(
        new GpuVideoDecoderHost(&router_, this, kContextRouteId,
                                kDecoderHostId));
    shared_memory_.reset(new base::SharedMemory());
    shared_memory_->CreateAnonymous(kTransportBufferSize);

    GpuVideoDecoderHostMsg_CreateVideoDecoderDone msg1(kDecoderHostId,
                                                       kDecoderId);
    EXPECT_CALL(*this, OnCreateVideoDecoder(kContextRouteId, kDecoderHostId))
        .WillOnce(SendMessage(decoder_host_.get(), msg1));

    GpuVideoDecoderInitDoneParam param;
    param.success = true;
    param.input_buffer_size = kTransportBufferSize;
    param.input_buffer_handle = shared_memory_->handle();

    GpuVideoDecoderHostMsg_InitializeACK msg2(kDecoderHostId, param);
    EXPECT_CALL(*this, OnInitialize(_))
        .WillOnce(SendMessage(decoder_host_.get(), msg2));
    EXPECT_CALL(*this, OnInitializeComplete(_));

    media::VideoCodecConfig config;
    config.codec = media::kCodecH264;
    config.width = kWidth;
    config.height = kHeight;
    decoder_host_->Initialize(&message_loop_, this, &context_, config);
    message_loop_.RunAllPending();
  }

  void Uninitialize() {
    // A message is sent to GPU process to destroy the decoder.
    GpuVideoDecoderHostMsg_DestroyACK msg(kDecoderHostId);
    EXPECT_CALL(*this, OnDestroy())
        .WillOnce(SendMessage(decoder_host_.get(), msg));
    EXPECT_CALL(context_, ReleaseAllVideoFrames());
    EXPECT_CALL(*this, OnUninitializeComplete());
    decoder_host_->Uninitialize();
  }

  void AllocateVideoFrames() {
    // Expect context is called to allocate video frames.
    EXPECT_CALL(context_,
                AllocateVideoFrames(kVideoFrames, kWidth, kHeight,
                                    media::VideoFrame::YV12,
                                    NotNull(), NotNull()))
        .WillOnce(SimulateAllocateVideoFrames(&frames_))
        .RetiresOnSaturation();

    // Expect that we send the video frames to the GPU process.
    EXPECT_CALL(*this, OnVideoFrameAllocated(_, _))
        .Times(kVideoFrames)
        .RetiresOnSaturation();

    // Pretend that a message is sent to GpuVideoDecoderHost to allocate
    // video frames.
    GpuVideoDecoderHostMsg_AllocateVideoFrames msg(
        kDecoderHostId, kVideoFrames, kWidth, kHeight,
        static_cast<int32>(media::VideoFrame::YV12));
    decoder_host_->OnMessageReceived(msg);
  }

  void ReleaseVideoFrames() {
    // Expect that context is called to release all video frames.
    EXPECT_CALL(context_, ReleaseAllVideoFrames())
        .RetiresOnSaturation();

    // Pretend a message is sent to release all video frames.
    GpuVideoDecoderHostMsg_ReleaseAllVideoFrames msg(kDecoderHostId);
    decoder_host_->OnMessageReceived(msg);

    // Clear the list of video frames allocated.
    frames_.clear();
  }

  void ProduceVideoFrame(int first_frame_id) {
    for (int i = 0; i < kVideoFrames; ++i) {
      // Expect that a request is received to produce a video frame.
      GpuVideoDecoderHostMsg_ConsumeVideoFrame msg(
          kDecoderHostId, first_frame_id + i, 0, 0, 0);
      EXPECT_CALL(*this, OnProduceVideoFrame(first_frame_id + i))
          .WillOnce(SendMessage(decoder_host_.get(), msg))
          .RetiresOnSaturation();

      // Expect that a reply is made when a video frame is ready.
      EXPECT_CALL(*this, ConsumeVideoFrame(frames_[i]))
          .RetiresOnSaturation();

      // Use the allocated video frames to make a request.
      decoder_host_->ProduceVideoFrame(frames_[i]);
    }
  }

 private:
  MessageLoop message_loop_;
  MessageRouter router_;
  media::MockVideoDecodeContext context_;

  scoped_ptr<GpuVideoDecoderHost> decoder_host_;
  scoped_ptr<base::SharedMemory> shared_memory_;

  // Keeps the video frames allocated.
  std::vector<scoped_refptr<media::VideoFrame> > frames_;
};

// Test that when we initialize GpuVideoDecoderHost the corresponding
// IPC messages are sent and at the end OnInitializeComplete() is
// called with the right parameters.
TEST_F(GpuVideoDecoderHostTest, Initialize) {
  Initialize();
}

// Test that the sequence of method calls and IPC messages is correct.
// And at the end OnUninitializeComplete() is called.
TEST_F(GpuVideoDecoderHostTest, Uninitialize) {
  Initialize();
  Uninitialize();
}

// Test that IPC messages are sent to GpuVideoDecoderHost and it
// calls VideoDecodeContext to allocate textures and send these
// textures back to the GPU process by IPC messages.
TEST_F(GpuVideoDecoderHostTest, AllocateVideoFrames) {
  Initialize();
  AllocateVideoFrames();
  Uninitialize();
}

// Test that IPC messages are sent to GpuVideoDecoderHost to
// release textures and VideoDecodeContext is called correctly.
TEST_F(GpuVideoDecoderHostTest, ReleaseVideoFrames) {
  Initialize();
  AllocateVideoFrames();
  ReleaseVideoFrames();
  AllocateVideoFrames();
  ReleaseVideoFrames();
  Uninitialize();
}

// Test the sequence of IPC messages and methods calls for a decode
// routine. This tests the output port only.
TEST_F(GpuVideoDecoderHostTest, ProduceVideoFrame) {
  Initialize();
  AllocateVideoFrames();
  ProduceVideoFrame(0);
  ReleaseVideoFrames();
  AllocateVideoFrames();
  ProduceVideoFrame(kVideoFrames);
  ReleaseVideoFrames();
  Uninitialize();
}
