// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stl_util-inl.h"
#include "media/base/data_buffer.h"
#include "media/base/mock_callback.h"
#include "media/base/mock_filter_host.h"
#include "media/base/mock_filters.h"
#include "media/filters/audio_renderer_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::StrictMock;

namespace media {

// Mocked subclass of AudioRendererBase for testing purposes.
class MockAudioRendererBase : public AudioRendererBase {
 public:
  MockAudioRendererBase()
      : AudioRendererBase() {}
  virtual ~MockAudioRendererBase() {}

  // AudioRenderer implementation.
  MOCK_METHOD1(SetVolume, void(float volume));

  // AudioRendererBase implementation.
  MOCK_METHOD1(OnInitialize, bool(const MediaFormat& media_format));
  MOCK_METHOD0(OnStop, void());

  // Used for verifying check points during tests.
  MOCK_METHOD1(CheckPoint, void(int id));

 private:
  FRIEND_TEST(AudioRendererBaseTest, OneCompleteReadCycle);

  DISALLOW_COPY_AND_ASSIGN(MockAudioRendererBase);
};

class AudioRendererBaseTest : public ::testing::Test {
 public:
  // Give the decoder some non-garbage media properties.
  AudioRendererBaseTest()
      : renderer_(new MockAudioRendererBase()),
        decoder_(new MockAudioDecoder()),
        pending_reads_(0) {
    renderer_->set_host(&host_);

    // Queue all reads from the decoder.
    EXPECT_CALL(*decoder_, ProduceAudioSamples(_))
        .WillRepeatedly(Invoke(this, &AudioRendererBaseTest::EnqueueCallback));

    // Sets the essential media format keys for this decoder.
    decoder_media_format_.SetAsInteger(MediaFormat::kChannels, 1);
    decoder_media_format_.SetAsInteger(MediaFormat::kSampleRate, 44100);
    decoder_media_format_.SetAsInteger(MediaFormat::kSampleBits, 16);
    EXPECT_CALL(*decoder_, media_format())
        .WillRepeatedly(ReturnRef(decoder_media_format_));
  }

  virtual ~AudioRendererBaseTest() {
    // Expect a call into the subclass.
    EXPECT_CALL(*renderer_, OnStop());
    renderer_->Stop(NewExpectedCallback());
  }

 protected:
  static const size_t kMaxQueueSize;

  // Fixture members.
  scoped_refptr<MockAudioRendererBase> renderer_;
  scoped_refptr<MockAudioDecoder> decoder_;
  StrictMock<MockFilterHost> host_;
  MediaFormat decoder_media_format_;

  // Number of asynchronous read requests sent to |decoder_|.
  size_t pending_reads_;

 private:
  void EnqueueCallback(scoped_refptr<Buffer> buffer) {
    ++pending_reads_;
  }

  DISALLOW_COPY_AND_ASSIGN(AudioRendererBaseTest);
};

const size_t AudioRendererBaseTest::kMaxQueueSize = 1u;

TEST_F(AudioRendererBaseTest, Initialize_Failed) {
  InSequence s;

  // Our subclass will fail when asked to initialize.
  EXPECT_CALL(*renderer_, OnInitialize(_))
      .WillOnce(Return(false));

  // We expect to receive an error.
  EXPECT_CALL(host_, SetError(PIPELINE_ERROR_INITIALIZATION_FAILED));

  // Initialize, we expect to have no reads.
  renderer_->Initialize(decoder_, NewExpectedCallback());
  EXPECT_EQ(0u, pending_reads_);
}

TEST_F(AudioRendererBaseTest, Initialize_Successful) {
  InSequence s;

  // Then our subclass will be asked to initialize.
  EXPECT_CALL(*renderer_, OnInitialize(_))
      .WillOnce(Return(true));

  // Initialize, we shouldn't have any reads.
  renderer_->Initialize(decoder_, NewExpectedCallback());
  EXPECT_EQ(0u, pending_reads_);

  // Now seek to trigger prerolling, verifying the callback hasn't been
  // executed yet.
  EXPECT_CALL(*renderer_, CheckPoint(0));
  renderer_->Seek(base::TimeDelta(), NewExpectedCallback());
  EXPECT_EQ(kMaxQueueSize, pending_reads_);
  renderer_->CheckPoint(0);

  // Now satisfy the read requests.  Our callback should be executed after
  // exiting this loop.
  while (pending_reads_) {
    scoped_refptr<DataBuffer> buffer(new DataBuffer(1024));
    buffer->SetDataSize(1024);
    --pending_reads_;
    decoder_->consume_audio_samples_callback()->Run(buffer);
  }
}

TEST_F(AudioRendererBaseTest, OneCompleteReadCycle) {
  InSequence s;

  // Then our subclass will be asked to initialize.
  EXPECT_CALL(*renderer_, OnInitialize(_))
      .WillOnce(Return(true));

  // Initialize, we shouldn't have any reads.
  renderer_->Initialize(decoder_, NewExpectedCallback());
  EXPECT_EQ(0u, pending_reads_);

  // Now seek to trigger prerolling, verifying the callback hasn't been
  // executed yet.
  EXPECT_CALL(*renderer_, CheckPoint(0));
  renderer_->Seek(base::TimeDelta(), NewExpectedCallback());
  EXPECT_EQ(kMaxQueueSize, pending_reads_);
  renderer_->CheckPoint(0);

  // Now satisfy the read requests.  Our callback should be executed after
  // exiting this loop.
  const uint32 kDataSize = 1024;
  uint32 bytes_buffered = 0;
  while (pending_reads_) {
    scoped_refptr<DataBuffer> buffer(new DataBuffer(kDataSize));
    buffer->SetDataSize(kDataSize);
    decoder_->consume_audio_samples_callback()->Run(buffer);
    --pending_reads_;
    bytes_buffered += kDataSize;
  }

  // Then set the renderer to play state.
  renderer_->Play(NewExpectedCallback());
  renderer_->SetPlaybackRate(1.0f);
  EXPECT_EQ(1.0f, renderer_->GetPlaybackRate());

  // Then flush the data in the renderer by reading from it.
  uint8 buffer[kDataSize];
  for (size_t i = 0; i < kMaxQueueSize; ++i) {
    EXPECT_EQ(kDataSize,
              renderer_->FillBuffer(buffer, kDataSize,
                                    base::TimeDelta(), true));
    bytes_buffered -= kDataSize;
  }

  // Make sure the read request queue is full.
  EXPECT_EQ(kMaxQueueSize, pending_reads_);

  // Fulfill the read with an end-of-stream packet.
  scoped_refptr<DataBuffer> last_buffer(new DataBuffer(0));
  decoder_->consume_audio_samples_callback()->Run(last_buffer);
  --pending_reads_;

  // We shouldn't report ended until all data has been flushed out.
  EXPECT_FALSE(renderer_->HasEnded());

  // We should have one less read request in the queue.
  EXPECT_EQ(kMaxQueueSize - 1, pending_reads_);

  // Flush the entire internal buffer and verify NotifyEnded() isn't called
  // right away.
  EXPECT_CALL(*renderer_, CheckPoint(1));
  EXPECT_EQ(0u, bytes_buffered % kDataSize);
  while (bytes_buffered > 0) {
    EXPECT_EQ(kDataSize,
              renderer_->FillBuffer(buffer, kDataSize,
                                    base::TimeDelta(), true));
    bytes_buffered -= kDataSize;
  }

  // Although we've emptied the buffer, we don't consider ourselves ended until
  // we request another buffer.  This way we know the last of the audio has
  // played.
  EXPECT_FALSE(renderer_->HasEnded());
  renderer_->CheckPoint(1);

  // Do an additional read to trigger NotifyEnded().
  EXPECT_CALL(host_, NotifyEnded());
  EXPECT_EQ(0u, renderer_->FillBuffer(buffer, kDataSize,
                                      base::TimeDelta(), true));

  // We should now report ended.
  EXPECT_TRUE(renderer_->HasEnded());

  // Further reads should return muted audio and not notify any more.
  EXPECT_EQ(0u, renderer_->FillBuffer(buffer, kDataSize,
                                      base::TimeDelta(), true));
}

}  // namespace media
