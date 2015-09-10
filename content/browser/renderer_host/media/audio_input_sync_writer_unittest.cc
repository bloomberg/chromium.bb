// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/sync_socket.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/media/audio_input_sync_writer.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "media/audio/audio_parameters.h"
#include "media/base/audio_bus.h"
#include "media/base/channel_layout.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using base::TimeDelta;
using media::AudioBus;
using media::AudioParameters;

namespace content {

namespace {

// Number of audio buffers in the faked ring buffer.
const int kSegments = 10;

// Audio buffer parameters.
const int channels = 1;
const int sampling_frequency_hz = 16000;
const int frames = sampling_frequency_hz / 100;  // 10 ms
const int bits_per_sample = 16;

// Faked ring buffer. Must be aligned.
#define DATA_ALIGNMENT 16
COMPILE_ASSERT(AudioBus::kChannelAlignment == DATA_ALIGNMENT,
               Data_alignment_not_same_as_AudioBus);
ALIGNAS(DATA_ALIGNMENT) uint8 data[kSegments *
    (sizeof(media::AudioInputBufferParameters) + frames * channels *
        sizeof(float))];

}  // namespace

// Mocked out sockets used for Send/ReceiveWithTimeout. Counts the number of
// outstanding reads, i.e. the diff between send and receive calls.
class MockCancelableSyncSocket : public base::CancelableSyncSocket {
 public:
  MockCancelableSyncSocket(int buffer_size)
      : in_failure_mode_(false),
        writes_(0),
        reads_(0),
        receives_(0),
        buffer_size_(buffer_size),
        read_buffer_index_(0) {}

  size_t Send(const void* buffer, size_t length) override {
    EXPECT_EQ(length, sizeof(uint32_t));

    ++writes_;
    EXPECT_LE(NumberOfBuffersFilled(), buffer_size_);
    return length;
  }

  size_t Receive(void* buffer, size_t length) override {
    EXPECT_EQ(0u, length % sizeof(uint32_t));

    if (in_failure_mode_)
      return 0;
    if (receives_ == reads_)
      return 0;

    uint32_t* ptr = static_cast<uint32_t*>(buffer);
    size_t received = 0;
    for (; received < length / sizeof(uint32_t) && receives_ < reads_;
         ++received, ++ptr) {
      ++receives_;
      EXPECT_LE(receives_, reads_);
      *ptr = ++read_buffer_index_;
    }
    return received * sizeof(uint32_t);
  }

  size_t Peek() override {
    return (reads_ - receives_) * sizeof(uint32_t);
  }

  // Simluates reading |buffers| number of buffers from the ring buffer.
  void Read(int buffers) {
    reads_ += buffers;
    EXPECT_LE(reads_, writes_);
  }

  // When |in_failure_mode_| == true, the socket fails to receive.
  void SetFailureMode(bool in_failure_mode) {
    in_failure_mode_ = in_failure_mode;
  }

  int NumberOfBuffersFilled() { return writes_ - reads_; }

 private:
  bool in_failure_mode_;
  int writes_;
  int reads_;
  int receives_;
  int buffer_size_;
  uint32_t read_buffer_index_;

  DISALLOW_COPY_AND_ASSIGN(MockCancelableSyncSocket);
};

class AudioInputSyncWriterUnderTest : public AudioInputSyncWriter {
 public:
  AudioInputSyncWriterUnderTest(void* shared_memory,
                                size_t shared_memory_size,
                                int shared_memory_segment_count,
                                const media::AudioParameters& params,
                                base::CancelableSyncSocket* socket)
      : AudioInputSyncWriter(shared_memory, shared_memory_size,
                             shared_memory_segment_count, params) {
    socket_.reset(socket);
  }

  ~AudioInputSyncWriterUnderTest() override {}

  MOCK_METHOD1(AddToNativeLog, void(const std::string& message));
};

class AudioInputSyncWriterTest : public testing::Test {
 public:
  AudioInputSyncWriterTest()
      : socket_(nullptr) {
    const media::ChannelLayout layout =
        media::GuessChannelLayout(channels);
    EXPECT_NE(media::ChannelLayout::CHANNEL_LAYOUT_UNSUPPORTED, layout);
    AudioParameters audio_params(
          AudioParameters::AUDIO_FAKE, layout, sampling_frequency_hz,
          bits_per_sample, frames);

    const uint32 segment_size =
        sizeof(media::AudioInputBufferParameters) +
        AudioBus::CalculateMemorySize(audio_params);
    size_t data_size = kSegments * segment_size;
    EXPECT_LE(data_size, sizeof(data));

    socket_ = new MockCancelableSyncSocket(kSegments);
    writer_.reset(new AudioInputSyncWriterUnderTest(
        &data[0], data_size, kSegments, audio_params, socket_));
    audio_bus_ = AudioBus::Create(audio_params);
  }

  ~AudioInputSyncWriterTest() override {
  }

  // Get total number of expected log calls. On non-Android we expect one log
  // call at first Write() call, zero on Android. Besides that only for errors.
  int GetTotalNumberOfExpectedLogCalls(int expected_calls_due_to_error) {
#if defined(OS_ANDROID)
    return expected_calls_due_to_error;
#else
    return expected_calls_due_to_error + 1;
#endif
  }

 protected:
  scoped_ptr<AudioInputSyncWriterUnderTest> writer_;
  MockCancelableSyncSocket* socket_;
  scoped_ptr<AudioBus> audio_bus_;

 private:
  TestBrowserThreadBundle thread_bundle_;

  DISALLOW_COPY_AND_ASSIGN(AudioInputSyncWriterTest);
};

TEST_F(AudioInputSyncWriterTest, SingleWriteAndRead) {
  // We always expect one log call at first write.
  EXPECT_CALL(*writer_.get(), AddToNativeLog(_))
      .Times(GetTotalNumberOfExpectedLogCalls(0));

  writer_->Write(audio_bus_.get(), 0, false, 0);
  EXPECT_EQ(1, socket_->NumberOfBuffersFilled());
  EXPECT_EQ(0u, socket_->Peek());

  socket_->Read(1);
  EXPECT_EQ(0, socket_->NumberOfBuffersFilled());
  EXPECT_EQ(sizeof(uint32_t), socket_->Peek());
}

TEST_F(AudioInputSyncWriterTest, MultipleWritesAndReads) {
  EXPECT_CALL(*writer_.get(), AddToNativeLog(_))
      .Times(GetTotalNumberOfExpectedLogCalls(0));

  for (int i = 1; i <= 2 * kSegments; ++i) {
    writer_->Write(audio_bus_.get(), 0, false, 0);
    EXPECT_EQ(1, socket_->NumberOfBuffersFilled());
    EXPECT_EQ(0u, socket_->Peek());

    socket_->Read(1);
    EXPECT_EQ(0, socket_->NumberOfBuffersFilled());
    EXPECT_EQ(sizeof(uint32_t), socket_->Peek());
  }
}

TEST_F(AudioInputSyncWriterTest, MultipleWritesNoReads) {
  EXPECT_CALL(*writer_.get(), AddToNativeLog(_))
      .Times(GetTotalNumberOfExpectedLogCalls(kSegments));

  for (int i = 1; i <= kSegments; ++i) {
    writer_->Write(audio_bus_.get(), 0, false, 0);
    EXPECT_EQ(i, socket_->NumberOfBuffersFilled());
    EXPECT_EQ(0u, socket_->Peek());
  }

  // Now the ring buffer is full, do more writes. We should get an extra error
  // log call for each write. See top EXPECT_CALL.
  for (int i = 1; i <= kSegments; ++i) {
    writer_->Write(audio_bus_.get(), 0, false, 0);
    EXPECT_EQ(kSegments, socket_->NumberOfBuffersFilled());
    EXPECT_EQ(0u, socket_->Peek());
  }
}

TEST_F(AudioInputSyncWriterTest, FillAndEmptyRingBuffer) {
  EXPECT_CALL(*writer_.get(), AddToNativeLog(_))
      .Times(GetTotalNumberOfExpectedLogCalls(1));

  // Fill ring buffer.
  for (int i = 1; i <= kSegments; ++i) {
    writer_->Write(audio_bus_.get(), 0, false, 0);
  }
  EXPECT_EQ(kSegments, socket_->NumberOfBuffersFilled());
  EXPECT_EQ(0u, socket_->Peek());

  // Empty half of the ring buffer.
  int buffers_to_read = kSegments / 2;
  socket_->Read(buffers_to_read);
  EXPECT_EQ(kSegments - buffers_to_read, socket_->NumberOfBuffersFilled());
  EXPECT_EQ(buffers_to_read * sizeof(uint32_t), socket_->Peek());

  // Fill up again. The first write should do receive until that queue is
  // empty.
  for (int i = kSegments - buffers_to_read + 1; i <= kSegments; ++i) {
    writer_->Write(audio_bus_.get(), 0, false, 0);
    EXPECT_EQ(i, socket_->NumberOfBuffersFilled());
    EXPECT_EQ(0u, socket_->Peek());
  }

  // Another write, should render and extra error log call.
  writer_->Write(audio_bus_.get(), 0, false, 0);
  EXPECT_EQ(kSegments, socket_->NumberOfBuffersFilled());
  EXPECT_EQ(0u, socket_->Peek());

  // Empty the ring buffer.
  socket_->Read(kSegments);
  EXPECT_EQ(0, socket_->NumberOfBuffersFilled());
  EXPECT_EQ(kSegments * sizeof(uint32_t), socket_->Peek());

  // Another write, should do receive until that queue is empty.
  writer_->Write(audio_bus_.get(), 0, false, 0);
  EXPECT_EQ(1, socket_->NumberOfBuffersFilled());
  EXPECT_EQ(0u, socket_->Peek());
}

}  // namespace content
