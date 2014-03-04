// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/test_helpers.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/pickle.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "media/base/audio_buffer.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/decoder_buffer.h"
#include "ui/gfx/rect.h"

using ::testing::_;
using ::testing::StrictMock;

namespace media {

// Utility mock for testing methods expecting Closures and PipelineStatusCBs.
class MockCallback : public base::RefCountedThreadSafe<MockCallback> {
 public:
  MockCallback();
  MOCK_METHOD0(Run, void());
  MOCK_METHOD1(RunWithStatus, void(PipelineStatus));

 protected:
  friend class base::RefCountedThreadSafe<MockCallback>;
  virtual ~MockCallback();

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCallback);
};

MockCallback::MockCallback() {}
MockCallback::~MockCallback() {}

base::Closure NewExpectedClosure() {
  StrictMock<MockCallback>* callback = new StrictMock<MockCallback>();
  EXPECT_CALL(*callback, Run());
  return base::Bind(&MockCallback::Run, callback);
}

PipelineStatusCB NewExpectedStatusCB(PipelineStatus status) {
  StrictMock<MockCallback>* callback = new StrictMock<MockCallback>();
  EXPECT_CALL(*callback, RunWithStatus(status));
  return base::Bind(&MockCallback::RunWithStatus, callback);
}

WaitableMessageLoopEvent::WaitableMessageLoopEvent()
    : message_loop_(base::MessageLoop::current()),
      signaled_(false),
      status_(PIPELINE_OK) {
  DCHECK(message_loop_);
}

WaitableMessageLoopEvent::~WaitableMessageLoopEvent() {}

base::Closure WaitableMessageLoopEvent::GetClosure() {
  DCHECK_EQ(message_loop_, base::MessageLoop::current());
  return BindToCurrentLoop(base::Bind(
      &WaitableMessageLoopEvent::OnCallback, base::Unretained(this),
      PIPELINE_OK));
}

PipelineStatusCB WaitableMessageLoopEvent::GetPipelineStatusCB() {
  DCHECK_EQ(message_loop_, base::MessageLoop::current());
  return BindToCurrentLoop(base::Bind(
      &WaitableMessageLoopEvent::OnCallback, base::Unretained(this)));
}

void WaitableMessageLoopEvent::RunAndWait() {
  RunAndWaitForStatus(PIPELINE_OK);
}

void WaitableMessageLoopEvent::RunAndWaitForStatus(PipelineStatus expected) {
  DCHECK_EQ(message_loop_, base::MessageLoop::current());
  if (signaled_) {
    EXPECT_EQ(expected, status_);
    return;
  }

  base::Timer timer(false, false);
  timer.Start(FROM_HERE, TestTimeouts::action_timeout(), base::Bind(
      &WaitableMessageLoopEvent::OnTimeout, base::Unretained(this)));

  message_loop_->Run();
  EXPECT_TRUE(signaled_);
  EXPECT_EQ(expected, status_);
}

void WaitableMessageLoopEvent::OnCallback(PipelineStatus status) {
  DCHECK_EQ(message_loop_, base::MessageLoop::current());
  signaled_ = true;
  status_ = status;
  message_loop_->QuitWhenIdle();
}

void WaitableMessageLoopEvent::OnTimeout() {
  DCHECK_EQ(message_loop_, base::MessageLoop::current());
  ADD_FAILURE() << "Timed out waiting for message loop to quit";
  message_loop_->QuitWhenIdle();
}

static VideoDecoderConfig GetTestConfig(VideoCodec codec,
                                        gfx::Size coded_size,
                                        bool is_encrypted) {
  gfx::Rect visible_rect(coded_size.width(), coded_size.height());
  gfx::Size natural_size = coded_size;

  return VideoDecoderConfig(codec, VIDEO_CODEC_PROFILE_UNKNOWN,
      VideoFrame::YV12, coded_size, visible_rect, natural_size,
      NULL, 0, is_encrypted);
}

static const gfx::Size kNormalSize(320, 240);
static const gfx::Size kLargeSize(640, 480);

VideoDecoderConfig TestVideoConfig::Invalid() {
  return GetTestConfig(kUnknownVideoCodec, kNormalSize, false);
}

VideoDecoderConfig TestVideoConfig::Normal() {
  return GetTestConfig(kCodecVP8, kNormalSize, false);
}

VideoDecoderConfig TestVideoConfig::NormalEncrypted() {
  return GetTestConfig(kCodecVP8, kNormalSize, true);
}

VideoDecoderConfig TestVideoConfig::Large() {
  return GetTestConfig(kCodecVP8, kLargeSize, false);
}

VideoDecoderConfig TestVideoConfig::LargeEncrypted() {
  return GetTestConfig(kCodecVP8, kLargeSize, true);
}

gfx::Size TestVideoConfig::NormalCodedSize() {
  return kNormalSize;
}

gfx::Size TestVideoConfig::LargeCodedSize() {
  return kLargeSize;
}

template <class T>
scoped_refptr<AudioBuffer> MakeAudioBuffer(SampleFormat format,
                                           int channels,
                                           T start,
                                           T increment,
                                           int frames,
                                           base::TimeDelta timestamp,
                                           base::TimeDelta duration) {
  scoped_refptr<AudioBuffer> output =
      AudioBuffer::CreateBuffer(format, channels, frames);
  output->set_timestamp(timestamp);
  output->set_duration(duration);

  // Create a block of memory with values:
  //   start
  //   start + increment
  //   start + 2 * increment, ...
  // For interleaved data, raw data will be:
  //   start
  //   start + channels * increment
  //   start + 2 * channels * increment, ...
  //
  // For planar data, values in channel 0 will be:
  //   start
  //   start + increment
  //   start + 2 * increment, ...
  // While, values in channel 1 will be:
  //   start + frames * increment
  //   start + (frames + 1) * increment
  //   start + (frames + 2) * increment, ...
  const size_t output_size =
      output->channel_data().size() == 1 ? frames * channels : frames;
  for (size_t ch = 0; ch < output->channel_data().size(); ++ch) {
    T* buffer = reinterpret_cast<T*>(output->channel_data()[ch]);
    for (size_t i = 0; i < output_size; ++i) {
      buffer[i] = start;
      start += increment;
    }
  }
  return output;
}

// Instantiate all the types of MakeAudioBuffer() and
// MakeAudioBuffer() needed.
#define DEFINE_MAKE_AUDIO_BUFFER_INSTANCE(type)              \
  template scoped_refptr<AudioBuffer> MakeAudioBuffer<type>( \
      SampleFormat format,                                   \
      int channels,                                          \
      type start,                                            \
      type increment,                                        \
      int frames,                                            \
      base::TimeDelta start_time,                            \
      base::TimeDelta duration)
DEFINE_MAKE_AUDIO_BUFFER_INSTANCE(uint8);
DEFINE_MAKE_AUDIO_BUFFER_INSTANCE(int16);
DEFINE_MAKE_AUDIO_BUFFER_INSTANCE(int32);
DEFINE_MAKE_AUDIO_BUFFER_INSTANCE(float);

static const char kFakeVideoBufferHeader[] = "FakeVideoBufferForTest";

scoped_refptr<DecoderBuffer> CreateFakeVideoBufferForTest(
    const VideoDecoderConfig& config,
    base::TimeDelta timestamp, base::TimeDelta duration) {
  Pickle pickle;
  pickle.WriteString(kFakeVideoBufferHeader);
  pickle.WriteInt(config.coded_size().width());
  pickle.WriteInt(config.coded_size().height());
  pickle.WriteInt64(timestamp.InMilliseconds());

  scoped_refptr<DecoderBuffer> buffer = DecoderBuffer::CopyFrom(
      static_cast<const uint8*>(pickle.data()),
      static_cast<int>(pickle.size()));
  buffer->set_timestamp(timestamp);
  buffer->set_duration(duration);

  return buffer;
}

bool VerifyFakeVideoBufferForTest(
    const scoped_refptr<DecoderBuffer>& buffer,
    const VideoDecoderConfig& config) {
  // Check if the input |buffer| matches the |config|.
  PickleIterator pickle(Pickle(reinterpret_cast<const char*>(buffer->data()),
                               buffer->data_size()));
  std::string header;
  int width = 0;
  int height = 0;
  bool success = pickle.ReadString(&header) && pickle.ReadInt(&width) &&
                 pickle.ReadInt(&height);
  return (success && header == kFakeVideoBufferHeader &&
          width == config.coded_size().width() &&
          height == config.coded_size().height());
}

}  // namespace media
