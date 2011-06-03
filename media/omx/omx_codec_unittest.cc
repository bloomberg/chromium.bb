// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define _CRT_SECURE_NO_WARNINGS

#if 0
#include <deque>

#include "base/callback.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "media/base/buffers.h"
#include "media/base/mock_filters.h"
#include "media/base/mock_task.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/video/omx_video_decode_engine.h"
#include "media/video/video_decode_engine.h"
#include "media/omx/mock_omx.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgumentPointee;
using ::testing::StrEq;
using ::testing::StrictMock;

namespace media {

static const int kBufferCount = 3;
static const int kBufferSize = 4096;
static const char* kComponentName = "OMX.google.video_decoder.avc";

ACTION(ReturnComponentName) {
  strcpy(((char**)arg2)[0], kComponentName);
}

ACTION(GetHandle) {
  *arg0 = MockOmx::get()->component();
  MockOmx::get()->component()->pApplicationPrivate = arg2;
  memcpy(MockOmx::get()->callbacks(), arg3, sizeof(OMX_CALLBACKTYPE));
}

ACTION(GetParameterVideoInit) {
  ((OMX_PORT_PARAM_TYPE*)arg1)->nStartPortNumber = 0;
}

ACTION(GetParameterPortDefinition) {
  OMX_PARAM_PORTDEFINITIONTYPE* port_format =
      (OMX_PARAM_PORTDEFINITIONTYPE*)arg1;
  CHECK(port_format->nPortIndex == 0 || port_format->nPortIndex == 1);
  if (port_format->nPortIndex == 0)
    port_format->eDir = OMX_DirInput;
  else
    port_format->eDir = OMX_DirOutput;
  port_format->nBufferCountMin = kBufferCount;
  port_format->nBufferCountActual = kBufferCount;
  port_format->nBufferSize = kBufferSize;
}

ACTION(AllocateBuffer) {
  *arg0 = new OMX_BUFFERHEADERTYPE();
  memset(*arg0, 0, sizeof(OMX_BUFFERHEADERTYPE));
  (*arg0)->nAllocLen = arg3;
  (*arg0)->pBuffer = new uint8[arg3];
  (*arg0)->nOutputPortIndex = 1;
  (*arg0)->nInputPortIndex = OMX_ALL;
}

ACTION(UseBuffer) {
  *arg0 = new OMX_BUFFERHEADERTYPE();
  memset(*arg0, 0, sizeof(OMX_BUFFERHEADERTYPE));
  (*arg0)->nOutputPortIndex = OMX_ALL;
  (*arg0)->nInputPortIndex = 0;
}

ACTION(FreeBuffer) {
  if (1 == arg1->nOutputPortIndex)
    delete [] arg1->pBuffer;
  delete arg1;
}

ACTION_P3(SendEvent, event, data1, data2) {
  (*MockOmx::get()->callbacks()->EventHandler)(
      MockOmx::get()->component(),
      MockOmx::get()->component()->pApplicationPrivate,
      event, static_cast<OMX_U32>(data1), static_cast<OMX_U32>(data2), NULL);
}

ACTION_P(EmptyBufferDone, output_pool_ptr) {
  (*MockOmx::get()->callbacks()->EmptyBufferDone)(
      MockOmx::get()->component(),
      MockOmx::get()->component()->pApplicationPrivate,
      arg0);
  OMX_BUFFERHEADERTYPE* out_buffer = output_pool_ptr->front();
  output_pool_ptr->pop_front();
  if (arg0->nFlags & OMX_BUFFERFLAG_EOS)
    out_buffer->nFlags |= OMX_BUFFERFLAG_EOS;
  out_buffer->nFilledLen = kBufferSize;
  (*MockOmx::get()->callbacks()->FillBufferDone)(
      MockOmx::get()->component(),
      MockOmx::get()->component()->pApplicationPrivate,
      out_buffer);
}

ACTION_P(EnqueueOutputBuffer, output_pool_ptr) {
  output_pool_ptr->push_back(arg0);
}

ACTION(FillEosBuffer) {
  arg0->nFlags = OMX_BUFFERFLAG_EOS;
  arg0->nFilledLen = 0;
  (*MockOmx::get()->callbacks()->FillBufferDone)(
      MockOmx::get()->component(),
      MockOmx::get()->component()->pApplicationPrivate,
      arg0);
}

class TestBuffer : public media::Buffer {
 public:
  TestBuffer() : size_(0), data_(NULL) { }
  explicit TestBuffer(int size) : size_(size) {
    if (size)
      data_.reset(new uint8[size]);
    else
      data_.reset(NULL);
  }
  virtual const uint8* GetData() const {
    return data_.get();
  }
  virtual size_t GetDataSize() const {
    return size_;
  }
 private:
  virtual ~TestBuffer() { }

  int size_;
  scoped_array<uint8> data_;
  DISALLOW_COPY_AND_ASSIGN(TestBuffer);
};

class OmxCodecTest : public testing::Test {
 public:
  OmxCodecTest ()
      : input_buffer_count_(0),
        got_eos_(false),
        omx_engine_(new OmxVideoDecodeEngine()) {
    av_stream_.codec = &av_codec_context_;
    av_codec_context_.width = 16;
    av_codec_context_.height = 16;
    feed_done_cb_ =
        NewCallback(this, &OmxCodecTest::EmptyBufferDoneCallback);
    decode_done_cb_ =
        NewCallback(this, &OmxCodecTest::FillBufferDoneCallback);
  }

  ~OmxCodecTest() {
  }

 protected:
  void ExpectSettings() {
    // Return the component name.
    EXPECT_CALL(*MockOmx::get(), GetComponentsOfRole(_, _, IsNull()))
        .WillOnce(DoAll(SetArgumentPointee<1>(1),
                        Return(OMX_ErrorNone)));
    EXPECT_CALL(*MockOmx::get(), GetComponentsOfRole(_, _, NotNull()))
        .WillOnce(DoAll(SetArgumentPointee<1>(1),
                        ReturnComponentName(),
                        Return(OMX_ErrorNone)));

    // Handle get parameter calls.
    EXPECT_CALL(*MockOmx::get(),
                GetParameter(OMX_IndexParamVideoInit, NotNull()))
        .WillRepeatedly(DoAll(GetParameterVideoInit(), Return(OMX_ErrorNone)));
    EXPECT_CALL(*MockOmx::get(),
                GetParameter(OMX_IndexParamPortDefinition, NotNull()))
        .WillRepeatedly(DoAll(GetParameterPortDefinition(),
                              Return(OMX_ErrorNone)));

    // Ignore all set parameter calls.
    EXPECT_CALL(*MockOmx::get(), SetParameter(_, _))
        .WillRepeatedly(Return(OMX_ErrorNone));
  }

  void ExpectToLoaded() {
    InSequence s;

    // Expect initialization.
    EXPECT_CALL(*MockOmx::get(), Init())
        .WillOnce(Return(OMX_ErrorNone));

    // Return the handle.
    EXPECT_CALL(*MockOmx::get(),
                GetHandle(NotNull(), StrEq(kComponentName),
                          NotNull(), NotNull()))
        .WillOnce(DoAll(GetHandle(),
                        Return(OMX_ErrorNone)));
  }

  void ExpectLoadedToIdle() {
    InSequence s;

    // Expect transition to idle.
    EXPECT_CALL(*MockOmx::get(),
                SendCommand(OMX_CommandStateSet, OMX_StateIdle, _))
        .WillOnce(
            DoAll(
                SendEvent(OMX_EventCmdComplete, OMX_CommandStateSet,
                          OMX_StateIdle),
                Return(OMX_ErrorNone)));

    // Expect allocation of buffers.
    EXPECT_CALL(*MockOmx::get(),
                UseBuffer(NotNull(), 0, NotNull(), kBufferSize, _))
        .Times(kBufferCount)
        .WillRepeatedly(DoAll(UseBuffer(), Return(OMX_ErrorNone)));

    // Expect allocation of output buffers and send command complete.
    EXPECT_CALL(*MockOmx::get(),
                AllocateBuffer(NotNull(), 1, IsNull(), kBufferSize))
        .Times(kBufferCount)
        .WillRepeatedly(DoAll(AllocateBuffer(), Return(OMX_ErrorNone)));
  }

  void ExpectToExecuting() {
    InSequence s;

    // Expect transition to executing.
    EXPECT_CALL(*MockOmx::get(),
                SendCommand(OMX_CommandStateSet, OMX_StateExecuting, _))
        .WillOnce(DoAll(
            SendEvent(OMX_EventCmdComplete, OMX_CommandStateSet,
                      OMX_StateExecuting),
            Return(OMX_ErrorNone)));

    // Expect initial FillThisBuffer() calls.
    EXPECT_CALL(*MockOmx::get(), FillThisBuffer(NotNull()))
        .Times(kBufferCount)
        .WillRepeatedly(DoAll(EnqueueOutputBuffer(&output_pool_),
                              Return(OMX_ErrorNone)));
  }

  void ExpectToIdle() {
    // Expect going to idle
    EXPECT_CALL(*MockOmx::get(),
                SendCommand(OMX_CommandStateSet, OMX_StateIdle, _))
        .WillOnce(DoAll(
            SendEvent(OMX_EventCmdComplete, OMX_CommandStateSet, OMX_StateIdle),
            Return(OMX_ErrorNone)));
  }

  void ExpectIdleToLoaded() {
    InSequence s;

    // Expect transition to loaded.
    EXPECT_CALL(*MockOmx::get(),
                SendCommand(OMX_CommandStateSet, OMX_StateLoaded, _))
        .WillOnce(DoAll(
            SendEvent(OMX_EventCmdComplete, OMX_CommandStateSet,
                      OMX_StateLoaded),
            Return(OMX_ErrorNone)));

    // Expect free buffer for input port.
    EXPECT_CALL(*MockOmx::get(), FreeBuffer(0, NotNull()))
        .Times(kBufferCount)
        .WillRepeatedly(DoAll(FreeBuffer(), Return(OMX_ErrorNone)));

    EXPECT_CALL(*MockOmx::get(), FreeBuffer(1, NotNull()))
        .Times(kBufferCount)
        .WillRepeatedly(DoAll(FreeBuffer(), Return(OMX_ErrorNone)));
  }

  void ExpectToEmpty() {
    InSequence s;

    EXPECT_CALL(*MockOmx::get(), FreeHandle(MockOmx::get()->component()))
        .WillOnce(Return(OMX_ErrorNone));
    EXPECT_CALL(*MockOmx::get(), Deinit())
        .WillOnce(Return(OMX_ErrorNone));
  }

  // TODO(hclam): Make a more generic about when to stop.
  void ExpectStart() {
    ExpectToLoaded();
    ExpectLoadedToIdle();
    ExpectToExecuting();
    EXPECT_CALL(init_done_cb_task_, Run());
  }

  void ExpectStop() {
    EXPECT_CALL(stop_task_, Run());
    ExpectToIdle();
    ExpectIdleToLoaded();
    ExpectToEmpty();
  }

  void EmptyBufferDoneCallback(scoped_refptr<Buffer> buffer) {
    if (buffer.get()) {
      input_units_.push_back(buffer);
    } else {
      input_buffer_count_++;
      scoped_refptr<Buffer> buffer_ref = new TestBuffer(input_buffer_count_);
      input_units_.push_back(buffer_ref);
    }
  }

  void FillBufferDoneCallback(scoped_refptr<VideoFrame> frame) {
    output_units_.push_back(frame);
    if (frame->IsEndOfStream())
      got_eos_ = true;
  }

  void MakeEmptyBufferRequest() {
    scoped_refptr<Buffer> buffer = input_units_.front();
    input_units_.pop_front();
    omx_engine_->EmptyThisBuffer(buffer);
  }

  void SendEOSInputBuffer() {
    input_units_.pop_front();
    scoped_refptr<Buffer> buffer_ref = new TestBuffer();
    input_units_.push_front(buffer_ref);
    EXPECT_CALL(*MockOmx::get(), EmptyThisBuffer(NotNull()))
        .WillOnce(DoAll(EmptyBufferDone(&output_pool_), Return(OMX_ErrorNone)))
        .RetiresOnSaturation();
    MakeEmptyBufferRequest();
    message_loop_.RunAllPending();
  }

  int input_buffer_count_;

  std::deque<scoped_refptr<Buffer> > input_units_;
  std::deque<scoped_refptr<VideoFrame> > output_units_;
  std::deque<OMX_BUFFERHEADERTYPE*> fill_this_buffer_received_;
  std::deque<OMX_BUFFERHEADERTYPE*> output_pool_;

  MockOmx mock_omx_;

  bool got_eos_;
  MessageLoop message_loop_;
  scoped_refptr<OmxVideoDecodeEngine> omx_engine_;

  AVStream av_stream_;
  AVCodecContext av_codec_context_;

  VideoDecodeEngine::ProduceVideoSampleCallback* feed_done_cb_;
  VideoDecodeEngine::ConsumeVideoFrameCallback* decode_done_cb_;
  TaskMocker init_done_cb_task_;

  TaskMocker stop_task_;

 private:
  DISALLOW_COPY_AND_ASSIGN(OmxCodecTest);
};

TEST_F(OmxCodecTest, SimpleStartAndStop) {
  ExpectSettings();
  ExpectStart();
  omx_engine_->Initialize(&message_loop_,
                          &av_stream_,
                          feed_done_cb_,
                          decode_done_cb_,
                          init_done_cb_task_.CreateTask());
  message_loop_.RunAllPending();

  EXPECT_EQ(kBufferCount, input_buffer_count_);
  EXPECT_EQ(VideoDecodeEngine::kNormal, omx_engine_->state());

  ExpectStop();
  omx_engine_->Stop(stop_task_.CreateTask());
  message_loop_.RunAllPending();
}

TEST_F(OmxCodecTest, NormalFlow) {
  ExpectSettings();
  ExpectStart();
  omx_engine_->Initialize(&message_loop_,
                          &av_stream_,
                          feed_done_cb_,
                          decode_done_cb_,
                          init_done_cb_task_.CreateTask());
  message_loop_.RunAllPending();

  EXPECT_EQ(kBufferCount, input_buffer_count_);
  EXPECT_EQ(VideoDecodeEngine::kNormal, omx_engine_->state());

  // Make emptybuffer requests.
  EXPECT_EQ(0u, output_units_.size());
  int count = output_pool_.size();
  for (int i = 0; i < kBufferCount; ++i) {
    // Give input buffers to OmxVideoDecodeEngine. OmxVideoDecodeEngine will
    // make a new FillThisBuffer() call for each read.
    EXPECT_CALL(*MockOmx::get(), EmptyThisBuffer(NotNull()))
        .WillOnce(DoAll(EmptyBufferDone(&output_pool_), Return(OMX_ErrorNone)))
        .RetiresOnSaturation();
    EXPECT_CALL(*MockOmx::get(), FillThisBuffer(NotNull()))
        .WillOnce(DoAll(EnqueueOutputBuffer(&output_pool_),
                        Return(OMX_ErrorNone)))
        .RetiresOnSaturation();
    MakeEmptyBufferRequest();
  }
  message_loop_.RunAllPending();
  EXPECT_EQ(kBufferCount, static_cast<int>(input_units_.size()));
  EXPECT_EQ(kBufferCount, static_cast<int>(output_units_.size()));
  EXPECT_EQ(count, static_cast<int>(output_pool_.size()));
  output_units_.clear();

  // Send EndOfStream, expect eos flag.
  SendEOSInputBuffer();
  EXPECT_EQ(kBufferCount - 1, static_cast<int>(input_units_.size()));
  EXPECT_EQ(1, static_cast<int>(output_units_.size()));
  EXPECT_EQ(count - 1, static_cast<int>(output_pool_.size()));
  EXPECT_TRUE(got_eos_);

  // Shutdown.
  ExpectStop();
  omx_engine_->Stop(stop_task_.CreateTask());
  message_loop_.RunAllPending();
}

TEST_F(OmxCodecTest, RecycleInputBuffers) {
  ExpectSettings();
  ExpectStart();
  omx_engine_->Initialize(&message_loop_,
                          &av_stream_,
                          feed_done_cb_,
                          decode_done_cb_,
                          init_done_cb_task_.CreateTask());
  message_loop_.RunAllPending();

  EXPECT_EQ(kBufferCount, input_buffer_count_);
  EXPECT_EQ(VideoDecodeEngine::kNormal, omx_engine_->state());

  // Make emptybuffer requests, also recycle input buffers
  EXPECT_EQ(0u, output_units_.size());
  int count = output_pool_.size();
  int repeat_count = kBufferCount * 2;
  for (int i = 0; i < repeat_count; ++i) {
    // Give input buffers to OmxVideoDecodeEngine. OmxVideoDecodeEngine will
    // make a new FillThisBuffer() call for each read.
    EXPECT_CALL(*MockOmx::get(), EmptyThisBuffer(NotNull()))
        .WillOnce(DoAll(EmptyBufferDone(&output_pool_), Return(OMX_ErrorNone)))
        .RetiresOnSaturation();
    EXPECT_CALL(*MockOmx::get(), FillThisBuffer(NotNull()))
        .WillOnce(DoAll(EnqueueOutputBuffer(&output_pool_),
                        Return(OMX_ErrorNone)))
        .RetiresOnSaturation();
    MakeEmptyBufferRequest();
    message_loop_.RunAllPending();
    CHECK(kBufferCount == static_cast<int>(input_units_.size()));
    CHECK(((i % kBufferCount) + 1) ==
          static_cast<int>(input_units_.back()->GetDataSize()));
  }
  message_loop_.RunAllPending();
  EXPECT_EQ(kBufferCount, static_cast<int>(input_units_.size()));
  EXPECT_EQ(repeat_count, static_cast<int>(output_units_.size()));
  EXPECT_EQ(count, static_cast<int>(output_pool_.size()));
  output_units_.clear();

  // Send EndOfStream, expect eos flag.
  SendEOSInputBuffer();
  EXPECT_EQ(kBufferCount - 1, static_cast<int>(input_units_.size()));
  EXPECT_EQ(1, static_cast<int>(output_units_.size()));
  EXPECT_EQ(count - 1, static_cast<int>(output_pool_.size()));
  EXPECT_TRUE(got_eos_);

  // Shutdown.
  ExpectStop();
  omx_engine_->Stop(stop_task_.CreateTask());
  message_loop_.RunAllPending();
}

// TODO(hclam): Add test case for dynamic port config.
// TODO(hclam): Create a more complicated test case so that read
// requests and reply from FillThisBuffer() arrives out of order.

}  // namespace media
#endif
