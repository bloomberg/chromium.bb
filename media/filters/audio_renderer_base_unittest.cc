// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stl_util-inl.h"
#include "media/base/data_buffer.h"
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
        decoder_(new MockAudioDecoder()) {
    renderer_->set_host(&host_);

    // Queue all reads from the decoder.
    EXPECT_CALL(*decoder_, Read(NotNull()))
        .WillRepeatedly(Invoke(this, &AudioRendererBaseTest::EnqueueCallback));

    // Sets the essential media format keys for this decoder.
    decoder_media_format_.SetAsString(MediaFormat::kMimeType,
                                      mime_type::kUncompressedAudio);
    decoder_media_format_.SetAsInteger(MediaFormat::kChannels, 1);
    decoder_media_format_.SetAsInteger(MediaFormat::kSampleRate, 44100);
    decoder_media_format_.SetAsInteger(MediaFormat::kSampleBits, 16);
    EXPECT_CALL(*decoder_, media_format())
        .WillRepeatedly(ReturnRef(decoder_media_format_));
  }

  virtual ~AudioRendererBaseTest() {
    STLDeleteElements(&read_queue_);

    // Expect a call into the subclass.
    EXPECT_CALL(*renderer_, OnStop());
    renderer_->Stop();
  }

 protected:
  static const size_t kMaxQueueSize;

  // Fixture members.
  scoped_refptr<MockAudioRendererBase> renderer_;
  scoped_refptr<MockAudioDecoder> decoder_;
  StrictMock<MockFilterHost> host_;
  StrictMock<MockFilterCallback> callback_;
  MediaFormat decoder_media_format_;

  // Receives asynchronous read requests sent to |decoder_|.
  std::deque<Callback1<Buffer*>::Type*> read_queue_;

 private:
  void EnqueueCallback(Callback1<Buffer*>::Type* callback) {
    read_queue_.push_back(callback);
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

  // We expect our callback to be executed.
  EXPECT_CALL(callback_, OnFilterCallback());
  EXPECT_CALL(callback_, OnCallbackDestroyed());

  // Initialize, we expect to have no reads.
  renderer_->Initialize(decoder_, callback_.NewCallback());
  EXPECT_EQ(0u, read_queue_.size());
}

TEST_F(AudioRendererBaseTest, Initialize_Successful) {
  InSequence s;

  // Then our subclass will be asked to initialize.
  EXPECT_CALL(*renderer_, OnInitialize(_))
      .WillOnce(Return(true));

  // After finishing initialization, we expect our callback to be executed.
  EXPECT_CALL(callback_, OnFilterCallback());
  EXPECT_CALL(callback_, OnCallbackDestroyed());

  // Verify the following expectations haven't run until we complete the reads.
  EXPECT_CALL(*renderer_, CheckPoint(0));

  MockFilterCallback seek_callback;
  EXPECT_CALL(seek_callback, OnFilterCallback());
  EXPECT_CALL(seek_callback, OnCallbackDestroyed());

  // Initialize, we shouldn't have any reads.
  renderer_->Initialize(decoder_, callback_.NewCallback());
  EXPECT_EQ(0u, read_queue_.size());

  // Now seek to trigger prerolling.
  renderer_->Seek(base::TimeDelta(), seek_callback.NewCallback());
  EXPECT_EQ(kMaxQueueSize, read_queue_.size());

  // Verify our seek callback hasn't been executed yet.
  renderer_->CheckPoint(0);

  // Now satisfy the read requests.  Our callback should be executed after
  // exiting this loop.
  while (!read_queue_.empty()) {
    scoped_refptr<DataBuffer> buffer = new DataBuffer(1024);
    buffer->SetDataSize(1024);
    read_queue_.front()->Run(buffer);
    delete read_queue_.front();
    read_queue_.pop_front();
  }
}

TEST_F(AudioRendererBaseTest, OneCompleteReadCycle) {
  InSequence s;

  // Then our subclass will be asked to initialize.
  EXPECT_CALL(*renderer_, OnInitialize(_))
      .WillOnce(Return(true));

  // After finishing initialization, we expect our callback to be executed.
  EXPECT_CALL(callback_, OnFilterCallback());
  EXPECT_CALL(callback_, OnCallbackDestroyed());

  // Verify the following expectations haven't run until we complete the reads.
  EXPECT_CALL(*renderer_, CheckPoint(0));

  MockFilterCallback seek_callback;
  EXPECT_CALL(seek_callback, OnFilterCallback());
  EXPECT_CALL(seek_callback, OnCallbackDestroyed());

  // Initialize, we shouldn't have any reads.
  renderer_->Initialize(decoder_, callback_.NewCallback());
  EXPECT_EQ(0u, read_queue_.size());

  // Now seek to trigger prerolling.
  renderer_->Seek(base::TimeDelta(), seek_callback.NewCallback());
  EXPECT_EQ(kMaxQueueSize, read_queue_.size());

  // Verify our seek callback hasn't been executed yet.
  renderer_->CheckPoint(0);

  // Now satisfy the read requests.  Our callback should be executed after
  // exiting this loop.
  const size_t kDataSize = 1024;
  size_t bytes_buffered = 0;
  while (!read_queue_.empty()) {
    scoped_refptr<DataBuffer> buffer = new DataBuffer(kDataSize);
    buffer->SetDataSize(kDataSize);
    read_queue_.front()->Run(buffer);
    delete read_queue_.front();
    read_queue_.pop_front();
    bytes_buffered += kDataSize;
  }

  MockFilterCallback play_callback;
  EXPECT_CALL(play_callback, OnFilterCallback());
  EXPECT_CALL(play_callback, OnCallbackDestroyed());

  // Then set the renderer to play state.
  renderer_->Play(play_callback.NewCallback());
  renderer_->SetPlaybackRate(1.0f);
  EXPECT_EQ(1.0f, renderer_->GetPlaybackRate());

  // Then flush the data in the renderer by reading from it.
  uint8 buffer[kDataSize];
  for (size_t i = 0; i < kMaxQueueSize; ++i) {
    EXPECT_EQ(kDataSize,
              renderer_->FillBuffer(buffer, kDataSize, base::TimeDelta()));
    bytes_buffered -= kDataSize;
  }

  // Make sure the read request queue is full.
  EXPECT_EQ(kMaxQueueSize, read_queue_.size());

  // Fulfill the read with an end-of-stream packet.
  scoped_refptr<DataBuffer> last_buffer = new DataBuffer(0);
  read_queue_.front()->Run(last_buffer);
  delete read_queue_.front();
  read_queue_.pop_front();

  // We shouldn't report ended until all data has been flushed out.
  EXPECT_FALSE(renderer_->HasEnded());

  // We should have one less read request in the queue.
  EXPECT_EQ(kMaxQueueSize - 1, read_queue_.size());

  // Flush the entire internal buffer and verify NotifyEnded() isn't called
  // right away.
  EXPECT_CALL(*renderer_, CheckPoint(1));
  EXPECT_EQ(0u, bytes_buffered % kDataSize);
  while (bytes_buffered > 0) {
    EXPECT_EQ(kDataSize,
              renderer_->FillBuffer(buffer, kDataSize, base::TimeDelta()));
    bytes_buffered -= kDataSize;
  }

  // Although we've emptied the buffer, we don't consider ourselves ended until
  // we request another buffer.  This way we know the last of the audio has
  // played.
  EXPECT_FALSE(renderer_->HasEnded());
  renderer_->CheckPoint(1);

  // Do an additional read to trigger NotifyEnded().
  EXPECT_CALL(host_, NotifyEnded());
  EXPECT_EQ(0u, renderer_->FillBuffer(buffer, kDataSize, base::TimeDelta()));

  // We should now report ended.
  EXPECT_TRUE(renderer_->HasEnded());

  // Further reads should return muted audio and not notify any more.
  EXPECT_EQ(0u, renderer_->FillBuffer(buffer, kDataSize, base::TimeDelta()));
}

}  // namespace media
